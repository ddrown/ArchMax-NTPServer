#include "main.h"
#include "lwip.h"
#include "jobs.h"
#include "uart.h"
#include "commandline.h"
#include "ntp.h"
#include "adc.h"
#include "ptp.h"
#include "timer.h"
#include "timesync.h"

enum jobids {NTP_POLL, ADC_POLL, UPDATE_ADC, PTP_SET_TARGET, PRINT_TIME, TIME_SYNC, ENDJOB};

static const struct jobdefs {
  enum jobids jobtype;
  uint16_t tick;
  uint8_t args;
} jobs[] = {
  { .jobtype= NTP_POLL,       .tick=   0, .args= 0 },
  { .jobtype= ADC_POLL,       .tick=   0, .args= 0 },
  { .jobtype= ADC_POLL,       .tick= 100, .args= 0 },
  { .jobtype= TIME_SYNC,      .tick= 150, .args= 0 },
  { .jobtype= ADC_POLL,       .tick= 200, .args= 0 },
  { .jobtype= ADC_POLL,       .tick= 300, .args= 0 },
  { .jobtype= ADC_POLL,       .tick= 400, .args= 0 },
  { .jobtype= PTP_SET_TARGET, .tick= 450, .args= 0 },
  { .jobtype= ADC_POLL,       .tick= 500, .args= 0 },
  { .jobtype= NTP_POLL,       .tick= 550, .args= 1 },
  { .jobtype= ADC_POLL,       .tick= 600, .args= 0 },
  { .jobtype= TIME_SYNC,      .tick= 650, .args= 0 },
  { .jobtype= ADC_POLL,       .tick= 700, .args= 0 },
  { .jobtype= ADC_POLL,       .tick= 800, .args= 0 },
  { .jobtype= ADC_POLL,       .tick= 900, .args= 0 },
  { .jobtype= UPDATE_ADC,     .tick= 901, .args= 0 },
  { .jobtype= PTP_SET_TARGET, .tick= 950, .args= 0 },
  { .jobtype= ENDJOB,         .tick= 999, .args= 0 },
};

static void runjob(uint8_t job) {
  switch(jobs[job].jobtype) {
    case NTP_POLL:
      ntp_poll(jobs[job].args);
      break;
    case ADC_POLL:
      adc_poll();
      break;
    case UPDATE_ADC:
      update_adc();
      break;
    case PTP_SET_TARGET:
      ptp_set_target();
      break;
    case PRINT_TIME:
      print_tim();
      break;
    case TIME_SYNC:
      time_sync();
      break;
    case ENDJOB:
      break;
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
