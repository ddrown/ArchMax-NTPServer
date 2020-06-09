
extern "C" {

#include "main.h"
#include "ptp.h"
#include "lwip/udp.h"
#include "uart.h"
#include "lwip/prot/ethernet.h"
};

#include "NTPServer.h"
#include "NTPClients.h"
#include "ntp.h"

NTPClients clientList;

extern "C" {
void TXTimestampCallback(uint32_t TimeStampLow, uint32_t TimeStampHigh, uint32_t length, uint8_t *packet) {
  struct eth_hdr *ethhdr = (struct eth_hdr *)packet;
  struct ip_hdr *iphdr;
  struct udp_hdr *udphdr;

  if(length < 90) {
    return;
  }

  if(ethhdr->type != PP_HTONS(ETHTYPE_IP)) {
    return;
  }

  iphdr = (struct ip_hdr *)(packet + sizeof(struct eth_hdr));

  if((iphdr->_proto != IP_PROTO_UDP) || (htons(iphdr->_len) < 76)) {
    return;
  }

  // assuming our sourced packets have no ip options
  udphdr = (struct udp_hdr *)(packet + sizeof(struct eth_hdr) + sizeof(struct ip_hdr));

  if(udphdr->src != PP_HTONS(NTP_PORT)) {
    return;
  }

  clientList.addTx(ntohl(iphdr->dest.addr), ntohs(udphdr->dest), TimeStampHigh, TimeStampLow << 1);
}

void ntp_init() {
  server.setup();
}

void expire_clients() {
  clientList.expireClients();
}
}
