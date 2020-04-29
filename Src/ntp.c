#include "main.h"
#include "ntp.h"
#include "uart.h"
#include "ptp.h"
#include "ethernetif.h"
#include "adc.h"

#include "lwip/udp.h"
#include <string.h>

static struct udp_pcb *ntp_pcb = NULL;

struct ntp_packet {
  // 0
  uint8_t leap_version_mode;
  uint8_t stratum;
  int8_t poll;
  int8_t precision;
  // 4
  uint16_t root_delay_s;
  uint16_t root_delay_subs;
  // 8
  uint16_t root_dispersion_s;
  uint16_t root_dispersion_subs;
  // 12
  uint32_t refid;
  uint32_t reftime_s;
  // 20
  uint32_t reftime_subs;
  uint32_t origin_s;
  // 28
  uint32_t origin_subs;
  uint32_t rx_s;
  // 36
  uint32_t rx_subs;
  uint32_t tx_s;
  // 44
  uint32_t tx_subs;
  // 48
};

struct ntp_server_destinations {
  const char *ipstr;
  ip_addr_t dest_addr;
  struct timestamp last_tx;
  struct timestamp start_ntp;
  struct timestamp end_ntp;
  uint32_t last_rx_s;
  uint32_t last_rx_subs;
};

static struct ntp_server_destinations dests[] = {
  {.ipstr = "10.1.2.198"},
  {.ipstr = "10.1.2.128"},
  {.ipstr = NULL},
}, *last_tx_dest = NULL;

static void ntp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const struct ip_addr *addr, u16_t port) {
  struct ntp_server_destinations *src = NULL;

  for(int i = 0; dests[i].ipstr != NULL; i++) {
    if(memcmp(&addr->u_addr.ip4, &dests[i].dest_addr.u_addr.ip4, sizeof(struct ip4_addr)) == 0) {
      src = &dests[i];
      break;
    }
  }

  if(src == NULL) {
    char srcaddr[IP4ADDR_STRLEN_MAX];
    ip4addr_ntoa_r(&addr->u_addr.ip4, srcaddr, IP4ADDR_STRLEN_MAX);
    write_uart_s("unknown source address ");
    write_uart_s(srcaddr);
    write_uart_s("\n");

    goto free_return;
  }

  ethernetif_scan_tx_timestamps(); // check for TX timestamps

  if(p->tot_len < sizeof(struct ntp_packet)) {
    write_uart_s("short packet: ");
    write_uart_u(p->tot_len);
    write_uart_s(" < ");
    write_uart_u(sizeof(struct ntp_packet));
    write_uart_s("\n");

    goto free_return;
  }

  if(p->len < sizeof(struct ntp_packet)) { // shouldn't ever happen
    write_uart_s("short fragment: ");
    write_uart_u(p->len);
    write_uart_s(" < ");
    write_uart_u(sizeof(struct ntp_packet));
    write_uart_s("\n");

    goto free_return;
  }

  struct ntp_packet *response = (struct ntp_packet *)p->payload;
  char type[3] = "? ";

  if(response->origin_s == htonl(src->end_ntp.seconds) &&
      response->origin_subs == htonl(src->end_ntp.subseconds*2)) {
    type[0] = 'I';
  } else if(response->origin_s == htonl(src->start_ntp.seconds) &&
      response->origin_subs == htonl(src->start_ntp.subseconds*2)) {
    type[0] = 'B';
  }

  src->end_ntp.seconds = p->timestamp_seconds;
  src->end_ntp.subseconds = p->timestamp_subseconds;

  src->last_rx_s = response->rx_s;
  src->last_rx_subs = response->rx_subs;

  // T  rtt   rxs        rxsub     txs        txsub      ends       endsub     offs  offsub     Ts         Tsub      T-start
  // I 354365 3762733097 431815355 3762733093 3366342847 3762467989 231917178 265107 2132235141 3762467989 231186357 14050
  write_uart_s(src->ipstr);
  write_uart_s(" ");
  write_uart_s(type);
  write_uart_u(ptp_ns_diff(&src->start_ntp, &src->end_ntp)); // RTT
  write_uart_s(" ");

  write_uart_u(ntohl(response->rx_s));
  write_uart_s(" ");
  write_uart_u(ntohl(response->rx_subs));
  write_uart_s(" ");
  write_uart_u(ntohl(response->tx_s));
  write_uart_s(" ");
  write_uart_u(ntohl(response->tx_subs));
  write_uart_s(" ");

  write_uart_u(src->end_ntp.seconds);
  write_uart_s(" ");
  write_uart_u(src->end_ntp.subseconds);
  write_uart_s(" ");

  int64_t offset_s = ntohl(response->rx_s) - (int64_t)src->start_ntp.seconds;
  int64_t offset_sub = ntohl(response->rx_subs)/2 - (int64_t)src->start_ntp.subseconds;

  if(offset_sub < 0 && offset_s > 0) {
    offset_s--;
    offset_sub += 2147483648LL;
  } else if(offset_sub > 0 && offset_s < 0) {
    offset_s++;
    offset_sub -= 2147483648LL;
  }

  write_uart_64i(offset_s);
  write_uart_s(" ");
  write_uart_64i(offset_sub);
  write_uart_s(" ");
  write_uart_u(src->last_tx.seconds);
  write_uart_s(" ");
  write_uart_u(src->last_tx.subseconds);
  write_uart_s(" ");
  write_uart_i(last_temp());
  write_uart_s(" ");
  if(src->last_tx.seconds != 0) {
    write_uart_u(ptp_ns_diff(&src->start_ntp, &src->last_tx));
    src->last_tx.seconds = 0;
    src->last_tx.subseconds = 0;
    write_uart_s("\n");
  } else {
    write_uart_s("-\n");
  }

free_return:
  pbuf_free(p);
}

void TXTimestampCallback(uint32_t TimeStampLow, uint32_t TimeStampHigh) {
  if(last_tx_dest != NULL) {
    last_tx_dest->last_tx.seconds = TimeStampHigh;
    last_tx_dest->last_tx.subseconds = TimeStampLow;
    last_tx_dest = NULL;
  }
}

static void ntp_send(struct ntp_server_destinations *dest) {
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
  request->origin_s = dest->last_rx_s;
  request->origin_subs = dest->last_rx_subs;
  request->rx_s = htonl(dest->end_ntp.seconds);
  request->rx_subs = htonl(dest->end_ntp.subseconds*2);

  ptp_timestamp(&dest->start_ntp);
  request->tx_s = htonl(dest->start_ntp.seconds);
  request->tx_subs = htonl(dest->start_ntp.subseconds*2);

  Eth_Timestamp_Next_Tx_Packet = 1;
  udp_sendto(ntp_pcb, p, &dest->dest_addr, 123);
  pbuf_free(p);
}

static uint8_t ntp_poll_active = 0;
void ntp_poll(uint8_t dest) {
  if(ntp_poll_active && dests[dest].ipstr != NULL) {
    last_tx_dest = &dests[dest];
    ntp_send(&dests[dest]);
  }
}

void ntp_poll_set(uint8_t active) {
  ntp_poll_active = active;
}

void ntp_init() {
  ntp_pcb = udp_new_ip_type(IPADDR_TYPE_ANY);
  udp_recv(ntp_pcb, ntp_recv, NULL);
  udp_bind(ntp_pcb, IP_ANY_TYPE, 1123);

  for(int i = 0; dests[i].ipstr != NULL; i++) {
    dests[i].end_ntp.seconds = 0;
    dests[i].end_ntp.subseconds = 0;
    dests[i].last_tx.seconds = 0;
    dests[i].last_tx.subseconds = 0;
    dests[i].last_rx_s = 0;
    dests[i].last_rx_subs = 0;
    IP_SET_TYPE_VAL(dests[i].dest_addr, IPADDR_TYPE_V4);
    if(!ip4addr_aton(dests[i].ipstr, ip_2_ip4(&dests[i].dest_addr))) {
      write_uart_s("ip4addr_aton failed on ");
      write_uart_s(dests[i].ipstr);
      write_uart_s("\n");
    }
  }
}
