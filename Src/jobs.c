#include "main.h"
#include "lwip.h"
#include "jobs.h"
#include "uart.h"
#include "commandline.h"
#include "ntp.h"
#include "ptp.h"
#include "timer.h"
#include "timesync.h"
#include "ethernetif.h"

enum jobids {STARTJOB, PTP_SET_TARGET, TIME_SYNC, SCAN_TX_TS, POLL_PHY, EXPIRE_CLIENTS, ENDJOB};
static void poll_phy_status();

static const struct jobdefs {
  enum jobids jobtype;
  uint16_t tick;
  uint8_t args;
} jobs[] = {
  { .jobtype= STARTJOB,       .tick=   0, .args= 0 },
  { .jobtype= TIME_SYNC,      .tick= 150, .args= 0 },
  { .jobtype= SCAN_TX_TS,     .tick= 250, .args= 0 },
  { .jobtype= POLL_PHY,       .tick= 350, .args= 0 },
  { .jobtype= PTP_SET_TARGET, .tick= 450, .args= 0 },
  { .jobtype= EXPIRE_CLIENTS, .tick= 550, .args= 0 },
  { .jobtype= TIME_SYNC,      .tick= 650, .args= 0 },
  { .jobtype= SCAN_TX_TS,     .tick= 750, .args= 0 },
  { .jobtype= PTP_SET_TARGET, .tick= 950, .args= 0 },
  { .jobtype= ENDJOB,         .tick= 999, .args= 0 },
};

static void runjob(uint8_t job) {
  switch(jobs[job].jobtype) {
    case PTP_SET_TARGET:
      ptp_set_target();
      break;
    case TIME_SYNC:
      time_sync();
      break;
    case SCAN_TX_TS:
      // check for TX timestamps if the RX previously timed out
      ethernetif_scan_tx_timestamps();
      break;
    case POLL_PHY:
      poll_phy_status();
      break;
    case EXPIRE_CLIENTS:
      expire_clients();
      break;
    case STARTJOB:
    case ENDJOB:
      break;
  }
}

static void poll_phy_status() {
  static uint8_t last_link = 0;
  uint32_t phyreg;

  if((HAL_ETH_ReadPHYRegister(&heth, PHY_SR, &phyreg)) != HAL_OK) {
    return;
  }

  if((phyreg & PHY_LINK_STATUS) != last_link) {
    last_link = phyreg & PHY_LINK_STATUS;
    if(last_link) {
      write_uart_s("link up\n");
      netif_set_link_up(&gnetif);
    } else {
      write_uart_s("link down\n");
      netif_set_link_down(&gnetif);
    }
  }
}

uint8_t ready_to_run(uint16_t tick, uint8_t job) {
  if(jobs[job].tick <= 100 && tick >= 900) {
    return 0;
  }
  if(jobs[job].tick <= tick) {
    return 1;
  }
  // allow jobs at the last 100ms to run in the first 100ms
  if(jobs[job].tick >= 900 && tick <= 100) {
    return 1;
  }
  return 0;
}

void runjobs() {
  static uint8_t nextjob = 0;
  static uint32_t last = 0;

  if(uart_rx_ready()) {
    cmdline_addchr(uart_rx_data());
  }
  MX_LWIP_Process();

  uint32_t now = HAL_GetTick();
  if(now != last) {
    uint16_t tick = now % 1000;
    while(ready_to_run(tick, nextjob)) {
      runjob(nextjob);
      nextjob++;
      if(jobs[nextjob].jobtype == ENDJOB) {
        nextjob = 0;
      }
    }
    last = now;
  }
}
