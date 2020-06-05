#include "main.h"
#include "ping.h"
#include "uart.h"
#include "ptp.h"

#include "lwip/raw.h"
#include "lwip/icmp.h"

// reference used: https://github.com/goertzenator/lwip/blob/master/contrib-1.4.0/apps/ping/ping.c

static struct raw_pcb *ping_pcb = NULL;
static struct timestamp start_ping;
static uint16_t ping_seq = 0;

#define DEFAULT_IPV4_HEADER_LEN 20

static unsigned char ping_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const struct ip_addr *addr) {
  struct timestamp end_ping;

  if(p->tot_len < DEFAULT_IPV4_HEADER_LEN+sizeof(struct icmp_echo_hdr)) {
    write_uart_s("short icmp packet\n");
    return 0;
  }

  uint8_t *packet = (uint8_t *)p->payload;
  struct ip_hdr *iph = (struct ip_hdr *)packet;
  if(IPH_V(iph) != 4) {
    return 0;
  }
  if(IPH_HL(iph) != DEFAULT_IPV4_HEADER_LEN/4) { // TODO: allow more IP headers?
    return 0;
  }

  // IPv4 header default size is 5*4=20 bytes
  struct icmp_echo_hdr *icmph = (struct icmp_echo_hdr *)(packet + DEFAULT_IPV4_HEADER_LEN);
  if(ICMPH_TYPE(icmph) != ICMP_ER) {
    return 0;
  }

  end_ping.seconds = p->ts.parts[TS_POS_S];
  end_ping.subseconds = p->ts.parts[TS_POS_SUBS];

  write_uart_s("rtt ");
  write_uart_u(ptp_ns_diff(&start_ping, &end_ping));
  write_uart_s(" ns\n");
  write_uart_s("id: ");
  write_uart_u(ntohs(icmph->id));
  write_uart_s(" seq: ");
  write_uart_u(ntohs(icmph->seqno));
  write_uart_s("\n");

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

  ptp_timestamp(&start_ping);
  raw_sendto(ping_pcb, p, &dest_addr);
  pbuf_free(p);
}

void ping_init() {
  ping_pcb = raw_new(IP_PROTO_ICMP);
  raw_recv(ping_pcb, ping_recv, NULL);
  raw_bind(ping_pcb, IP_ADDR_ANY);
}
