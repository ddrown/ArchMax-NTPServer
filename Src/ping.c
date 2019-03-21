#include "main.h"
#include "ping.h"
#include "uart.h"

#include "lwip/raw.h"
#include "lwip/icmp.h"

// reference used: https://github.com/goertzenator/lwip/blob/master/contrib-1.4.0/apps/ping/ping.c

static struct raw_pcb *ping_pcb = NULL;
static uint32_t start_ping = 0;
static uint16_t ping_seq = 0;

static unsigned char ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const struct ip_addr *addr) {
  uint32_t pingrtt = HAL_GetTick() - start_ping;
  uint8_t *packet = (uint8_t *)p->payload;
  struct ip_hdr *iph = (struct ip_hdr *)packet;
  if(IPH_V(iph) != 4) {
    return 0;
  }
  if(IPH_HL(iph) != 5) { // TODO: allow more IP headers?
    return 0;
  }

  struct icmp_echo_hdr *icmph = (struct icmp_echo_hdr *)(packet + 20);
  if(ICMPH_TYPE(icmph) != ICMP_ER) {
    return 0;
  }

  write_uart_s("rtt ");
  write_uart_u(pingrtt);
  write_uart_s(" ms\n");
  write_uart_s("id: ");
  write_uart_u(ntohs(icmph->id));
  write_uart_s(" seq: ");
  write_uart_u(ntohs(icmph->seqno));
  write_uart_s("\n");
  start_ping = 0;

  pbuf_free(p);
  return 1;
}

void ping_send(const char *dest) {
  ip_addr_t dest_addr;

  IP_SET_TYPE_VAL(dest_addr, IPADDR_TYPE_V4);
  if(!ip4addr_aton(dest, ip_2_ip4(&dest_addr))) {
    write_uart_s("ip4addr_aton failed\n");
    return;
  }

  struct pbuf *p = pbuf_alloc(PBUF_IP, sizeof(struct icmp_echo_hdr), PBUF_RAM);
  if(!p) {
    write_uart_s("pbuf_alloc failed\n");
    return;
  }

  struct icmp_echo_hdr *icmph = (struct icmp_echo_hdr *)p->payload;

  ICMPH_TYPE_SET(icmph, ICMP_ECHO);
  ICMPH_CODE_SET(icmph, 0);
  icmph->chksum = 0;
  icmph->id     = htons(12345);
  icmph->seqno  = htons(ping_seq++);

  start_ping = HAL_GetTick();
  raw_sendto(ping_pcb, p, &dest_addr);
  pbuf_free(p);
}

void ping_init() {
  ping_pcb = raw_new(IP_PROTO_ICMP);
  raw_recv(ping_pcb, ping_recv, NULL);
  raw_bind(ping_pcb, IP_ADDR_ANY);
}
