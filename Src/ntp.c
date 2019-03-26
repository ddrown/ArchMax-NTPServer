#include "main.h"
#include "ntp.h"
#include "uart.h"
#include "ptp.h"

#include "lwip/udp.h"
#include <string.h>

static struct udp_pcb *ntp_pcb = NULL;
static struct timestamp start_ntp;

struct ntp_packet {
  uint8_t leap_version_mode;
  uint8_t stratum;
  int8_t poll;
  int8_t precision;
  uint16_t root_delay_s;
  uint16_t root_delay_subs;
  uint16_t root_dispersion_s;
  uint16_t root_dispersion_subs;
  uint32_t refid;
  uint32_t reftime_s;
  uint32_t reftime_subs;
  uint32_t origin_s;
  uint32_t origin_subs;
  uint32_t rx_s;
  uint32_t rx_subs;
  uint32_t tx_s;
  uint32_t tx_subs;
};

static void ntp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const struct ip_addr *addr, u16_t port) {
  if(p->tot_len < sizeof(struct ntp_packet)) {
    write_uart_s("short packet: ");
    write_uart_u(p->tot_len);
    write_uart_s(" < ");
    write_uart_u(sizeof(struct ntp_packet));
    write_uart_s("\n");

    pbuf_free(p);
    return;
  }

  if(p->len < sizeof(struct ntp_packet)) { // shouldn't ever happen
    write_uart_s("short fragment: ");
    write_uart_u(p->len);
    write_uart_s(" < ");
    write_uart_u(sizeof(struct ntp_packet));
    write_uart_s("\n");

    pbuf_free(p);
    return;
  }

  struct timestamp end_ntp;
  struct ntp_packet *response = (struct ntp_packet *)p->payload;

  end_ntp.seconds = p->timestamp_seconds;
  end_ntp.subseconds = p->timestamp_subseconds;

  write_uart_u(ptp_ns_diff(&start_ntp, &end_ntp));
  write_uart_s(",");

  write_uart_u(ntohl(response->rx_s));
  write_uart_s(",");
  write_uart_u(ntohl(response->rx_subs));
  write_uart_s(",");
  write_uart_u(ntohl(response->tx_s));
  write_uart_s(",");
  write_uart_u(ntohl(response->tx_subs));
  write_uart_s(",");

  write_uart_u(end_ntp.seconds);
  write_uart_s(",");
  write_uart_u(end_ntp.subseconds);
  write_uart_s("\n");

  int64_t offset_s = ntohl(response->tx_s) - (int64_t)end_ntp.seconds;
  int64_t offset_sub = ntohl(response->tx_subs) - ((int64_t)end_ntp.subseconds)*2;

  if(offset_sub < 0 && offset_s > 0) {
    offset_s--;
    offset_sub += 4294967296LL;
  } else if(offset_sub > 0 && offset_s < 0) {
    offset_s++;
    offset_sub -= 4294967296LL;
  }

  write_uart_s("offset ");
  write_uart_64i(offset_s);
  write_uart_s(",");
  write_uart_64i(offset_sub);
  write_uart_s("\n");

  pbuf_free(p);
}

void ntp_send(const char *dest) {
  ip_addr_t dest_addr;

  IP_SET_TYPE_VAL(dest_addr, IPADDR_TYPE_V4);
  if(!ip4addr_aton(dest, ip_2_ip4(&dest_addr))) {
    write_uart_s("ip4addr_aton failed\n");
    return;
  }

  struct pbuf *p = pbuf_alloc(PBUF_IP, sizeof(struct ntp_packet), PBUF_RAM);
  if(!p) {
    write_uart_s("pbuf_alloc failed\n");
    return;
  }

  struct ntp_packet *request = (struct ntp_packet *)p->payload;
  memset(request, 0, sizeof(struct ntp_packet));
  request->leap_version_mode = 0b11100011;   // LI=unsync, Version=4, Mode=3(client)
  request->poll = 6; // 2^6 = 64s
  request->precision = -27;  // Peer Clock Precision in 2^x seconds, -27=6ns

  ptp_timestamp(&start_ntp);
  udp_sendto(ntp_pcb, p, &dest_addr, 123);
  pbuf_free(p);
}

static uint8_t ntp_poll_active = 0;
void ntp_poll() {
  if(ntp_poll_active) {
    ntp_send("10.1.2.198");
  }
}

void ntp_poll_set(uint8_t active) {
  ntp_poll_active = active;
}

void ntp_init() {
  ntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
  udp_recv(ntp_pcb, ntp_recv, NULL);
  udp_bind(ntp_pcb, IP_ANY_TYPE, 123);
}
