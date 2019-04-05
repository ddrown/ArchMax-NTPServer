#include "main.h"
#include "ntp.h"
#include "uart.h"
#include "ptp.h"
#include "ethernetif.h"

#include "lwip/udp.h"
#include <string.h>

static struct udp_pcb *ntp_pcb = NULL;
static struct timestamp start_ntp;

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

static uint32_t last_rx_s = 0;
static uint32_t last_rx_subs = 0;
static struct timestamp end_ntp = {.seconds = 0, .subseconds = 0};
static struct timestamp last_tx = {.seconds = 0, .subseconds = 0}; 

static void ntp_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const struct ip_addr *addr, u16_t port) {
  struct timestamp rx_done;
  ptp_timestamp(&rx_done);
  ethernetif_scan_tx_timestamps(); // check for TX timestamps

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

  struct ntp_packet *response = (struct ntp_packet *)p->payload;
  char type[3] = "? ";

  if(response->origin_s == htonl(end_ntp.seconds) && 
      response->origin_subs == htonl(end_ntp.subseconds*2)) {
    type[0] = 'I';
  } else if(response->origin_s == htonl(start_ntp.seconds) && 
      response->origin_subs == htonl(start_ntp.subseconds*2)) {
    type[0] = 'B';
  }


  end_ntp.seconds = p->timestamp_seconds;
  end_ntp.subseconds = p->timestamp_subseconds;

  last_rx_s = response->rx_s;
  last_rx_subs = response->rx_subs;

  // T  rtt   rxs        rxsub     txs        txsub      ends       endsub     offs  offsub     Ts         Tsub      T-start
  // I 354365 3762733097 431815355 3762733093 3366342847 3762467989 231917178 265107 2132235141 3762467989 231186357 14050
  write_uart_s(type);
  write_uart_u(ptp_ns_diff(&start_ntp, &end_ntp));
  write_uart_s(" ");

  write_uart_u(ntohl(response->rx_s));
  write_uart_s(" ");
  write_uart_u(ntohl(response->rx_subs));
  write_uart_s(" ");
  write_uart_u(ntohl(response->tx_s));
  write_uart_s(" ");
  write_uart_u(ntohl(response->tx_subs));
  write_uart_s(" ");

  write_uart_u(end_ntp.seconds);
  write_uart_s(" ");
  write_uart_u(end_ntp.subseconds);
  write_uart_s(" ");

  int64_t offset_s = ntohl(response->rx_s) - (int64_t)start_ntp.seconds;
  int64_t offset_sub = ntohl(response->rx_subs)/2 - (int64_t)start_ntp.subseconds;

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
  write_uart_u(last_tx.seconds);
  write_uart_s(" ");
  write_uart_u(last_tx.subseconds);
  write_uart_s(" ");
  write_uart_u(ptp_ns_diff(&end_ntp, &rx_done));
  write_uart_s(" ");
  if(last_tx.seconds != 0) {
    write_uart_u(ptp_ns_diff(&start_ntp, &last_tx));
    last_tx.seconds = 0;
    last_tx.subseconds = 0;
    write_uart_s("\n");
  } else {
    write_uart_s("-\n");
  }

  pbuf_free(p);
}

void TXTimestampCallback(uint32_t TimeStampLow, uint32_t TimeStampHigh) {
  last_tx.seconds = TimeStampHigh;
  last_tx.subseconds = TimeStampLow;
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
  request->origin_s = last_rx_s;
  request->origin_subs = last_rx_subs;
  request->rx_s = htonl(end_ntp.seconds);
  request->rx_subs = htonl(end_ntp.subseconds*2);

  ptp_timestamp(&start_ntp);
  request->tx_s = htonl(start_ntp.seconds);
  request->tx_subs = htonl(start_ntp.subseconds*2);

  // TODO: ARP?
  Eth_Timestamp_Next_Tx_Packet = 1;
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
