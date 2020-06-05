#include "main.h"
#include "ptp.h"
#include "lwip/udp.h"
#include "NTPServer.h"

extern "C" {

#include "ntp.h"

void TXTimestampCallback(uint32_t TimeStampLow, uint32_t TimeStampHigh) {
  UNUSED(TimeStampLow);
  UNUSED(TimeStampHigh);
// TODO
// TODO: ethernetif_scan_tx_timestamps();
// TODO: Eth_Timestamp_Next_Tx_Packet = 1;
}

void ntp_init() {
  server.setup();
}
}
