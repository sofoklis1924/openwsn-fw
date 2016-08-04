#include "opendefs.h"
#include "userialbridge.h"
#include "openudp.h"
#include "openqueue.h"
#include "opentimers.h"
#include "openserial.h"
#include "packetfunctions.h"
#include "scheduler.h"
#include "IEEE802154E.h"
#include "idmanager.h"

//=========================== variables =======================================

userialbridge_vars_t userialbridge_vars;

static const uint8_t userialbridge_dst_addr[]   = {
   0xbb, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
}; 

//=========================== prototypes ======================================

void userialbridge_timer_cb(opentimer_id_t id);
void userialbridge_task_cb(void);

//=========================== public ==========================================

void userialbridge_init() {
   
   // clear local variables
   memset(&userialbridge_vars,0,sizeof(userialbridge_vars_t));
   
   userialbridge_vars.period = USERIALBRIDGE_PERIOD_MS;
   
   // start periodic timer
   userialbridge_vars.timerId                    = opentimers_start(
      userialbridge_vars.period,
      TIMER_PERIODIC,TIME_MS,
      userialbridge_timer_cb
   );
}

void userialbridge_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
   openqueue_freePacketBuffer(msg);
}

void userialbridge_receive(OpenQueueEntry_t* pkt) {
   
   openqueue_freePacketBuffer(pkt);
   
   openserial_printError(
      COMPONENT_USERIALBRIDGE,
      ERR_RCVD_ECHO_REPLY,
      (errorparameter_t)0,
      (errorparameter_t)0
   );
}

//=========================== private =========================================

/**
\note timer fired, but we don't want to execute task in ISR mode instead, push
   task to scheduler with CoAP priority, and let scheduler take care of it.
*/
void userialbridge_timer_cb(opentimer_id_t id){
   
   scheduler_push_task(userialbridge_task_cb,TASKPRIO_COAP);
}

void userialbridge_task_cb() {
   OpenQueueEntry_t*    pkt;
   uint8_t              asnArray[5];
   
   // don't run if not synch
   if (ieee154e_isSynch() == FALSE) return;
   
   // don't run on dagroot
   if (idmanager_getIsDAGroot()) {
      opentimers_stop(userialbridge_vars.timerId);
      return;
   }
   
   // if you get here, send a packet
   
   // get a free packet buffer
   pkt = openqueue_getFreePacketBuffer(COMPONENT_USERIALBRIDGE);
   if (pkt==NULL) {
      openserial_printError(
         COMPONENT_USERIALBRIDGE,
         ERR_NO_FREE_PACKET_BUFFER,
         (errorparameter_t)0,
         (errorparameter_t)0
      );
      return;
   }
   
   pkt->owner                         = COMPONENT_USERIALBRIDGE;
   pkt->creator                       = COMPONENT_USERIALBRIDGE;
   pkt->l4_protocol                   = IANA_UDP;
   pkt->l4_destination_port           = WKP_UDP_SERIALBRIDGE;
   pkt->l4_sourcePortORicmpv6Type     = WKP_UDP_SERIALBRIDGE;
   pkt->l3_destinationAdd.type        = ADDR_128B;
   memcpy(&pkt->l3_destinationAdd.addr_128b[0],userialbridge_dst_addr,16);
   
   packetfunctions_reserveHeaderSize(pkt,sizeof(uint16_t));
   pkt->payload[1] = (uint8_t)((userialbridge_vars.counter & 0xff00)>>8);
   pkt->payload[0] = (uint8_t)(userialbridge_vars.counter & 0x00ff);
   userialbridge_vars.counter++;
   
   packetfunctions_reserveHeaderSize(pkt,sizeof(asn_t));
   ieee154e_getAsn(asnArray);
   pkt->payload[0] = asnArray[0];
   pkt->payload[1] = asnArray[1];
   pkt->payload[2] = asnArray[2];
   pkt->payload[3] = asnArray[3];
   pkt->payload[4] = asnArray[4];
   
   if ((openudp_send(pkt))==E_FAIL) {
      openqueue_freePacketBuffer(pkt);
   }
}
