extern "C" {
#include "main.h"
#include "GPS.h"
#include "timer.h"
#include "timesync.h"
#include "uart.h"
#include "ptp.h"
#include <ctype.h>
}

#include "DateTime.h"
#include "NTPClock.h"
#include "NTPClockCMD.h"
#include "ClockPID.h"
#include "NTPServer.h"

ClockPID_c ClockPID;
ClockPID_c PTPPID;

static bool time_set = false;
static uint8_t ptp_time_set = 0;
static uint32_t lastpps = 0;
static uint32_t lastgpstime = 0;
static int64_t last_local_offset = 0;
static const float billion = 1000000000.0;

const DateTime compile = DateTime(__DATE__, __TIME__);

extern "C" {

void show_pid() {
  write_uart_s("[TIM2] P=");
  write_uart_u(ClockPID.get_kp()*1000000.0f);
  write_uart_s(" I=");
  write_uart_u(ClockPID.get_ki()*1000000.0f);
  write_uart_s(" D=");
  write_uart_u(ClockPID.get_kd()*1000000.0f);
  write_uart_s("[PTP] P=");
  write_uart_u(PTPPID.get_kp()*1000000.0f);
  write_uart_s(" I=");
  write_uart_u(PTPPID.get_ki()*1000000.0f);
  write_uart_s(" D=");
  write_uart_u(PTPPID.get_kd()*1000000.0f);
  write_uart_s("\n");
}

void set_pid_constant(char type, uint32_t millionth) {
  float newvalue = millionth / 1000000.0f;
  switch(type) {
    case 'p':
      ClockPID.set_kp(newvalue);
      break;
    case 'i':
      ClockPID.set_ki(newvalue);
      break;
    case 'd':
      ClockPID.set_kd(newvalue);
      break;
    case 'P':
      PTPPID.set_kp(newvalue);
      break;
    case 'I':
      PTPPID.set_ki(newvalue);
      break;
    case 'D':
      PTPPID.set_kd(newvalue);
      break;
    default:
      write_uart_s("unknown type ");
      write_uart_ch(type);
      write_uart_s("\n");
      break;
  }

  show_pid();
}

#define PTP_OFFSET_HISTORY_MAX 60
static int64_t ptp_offset_history[PTP_OFFSET_HISTORY_MAX];
static uint8_t ptp_offset_len = 0;
static void add_ptp_history(int64_t offset) {
  if(ptp_offset_len == PTP_OFFSET_HISTORY_MAX) {
    for(uint8_t i = 0; i < PTP_OFFSET_HISTORY_MAX-1; i++) {
      ptp_offset_history[i] = ptp_offset_history[i+1];
    }
    ptp_offset_len--;
  }
  ptp_offset_history[ptp_offset_len] = offset;
  ptp_offset_len++;
}

static int64_t max_ptp_history() {
  if(ptp_offset_len == 0) {
    return 0;
  }
  int64_t max = ptp_offset_history[0];
  for(uint8_t i = 1; i < ptp_offset_len; i++) {
    if(ptp_offset_history[i] > max)
      max = ptp_offset_history[i];
  }
  return max;
}

static uint32_t ntp64_to_32(int64_t offset) {
  if(offset < 0)
    offset *= -1;
  // take 16bits off the bottom and top
  offset = offset >> 16;
  return offset & 0xffffffff;
}

static uint32_t last_ptp_seconds = 0;
static void compare_ptp() {
  uint32_t thisptp_seconds = pps_capture.ptp_seconds;
  uint32_t thisptp_cap = pps_capture.ptp_cap;
  uint32_t dispersion_adjust = 0;

  if(thisptp_seconds == last_ptp_seconds) {
    return;
  }
  last_ptp_seconds = thisptp_seconds;

  // have we been without a gps PPS for longer than 1 second?
  if(thisptp_seconds > lastgpstime+1) {
    // estimate: 1us/s of drift
    dispersion_adjust = (thisptp_seconds - lastgpstime) / 15;
  }

  int64_t offset = localClock.getOffset(thisptp_cap, thisptp_seconds, 0);
  offset -= last_local_offset;

  add_ptp_history(offset);

  if(time_set && ptp_time_set < 2) {
    ptp_time_set++;
    if(ptp_time_set == 2) {
      ptp_update_subs(offset / -2);
      write_uart_s("ptp init ");
      write_uart_64i(offset);
      write_uart_s("\n");
      PTPPID.set_kp(1);
      PTPPID.set_ki(0.006);
    }
    return;
  }

  server.setDispersion(dispersion_adjust + ntp64_to_32(max_ptp_history()));

  PTPPID.add_sample(thisptp_cap, thisptp_seconds, offset);

  int32_t newppb = (ClockPID.d_out() - PTPPID.p_out() - PTPPID.i_out()) * billion;

  ptp_set_freq_div(newppb);

#ifdef DEBUG_SERIAL
  write_uart_s("ptp: ");
  write_uart_64i(offset);
  write_uart_s(" P=");
  write_uart_i(PTPPID.p_out() * billion);
  write_uart_s(" I=");
  write_uart_i(PTPPID.i_out() * billion);
  write_uart_s(" D=");
  write_uart_i(PTPPID.d_out() * billion);
  write_uart_s(" ppb=");
  write_uart_i(newppb);
  write_uart_s(" T=");
  write_uart_u(thisptp_seconds);
  write_uart_s("\n");
#endif
}

void time_sync() {
  uint32_t thispps, thisgpstime;
  uint32_t thispps_millis, thisgps_millis;

  thispps = get_last_pps();
  thisgpstime = gps_time();
  thispps_millis = get_last_pps_millis();
  thisgps_millis = gps_time_millis();
  int32_t pps_to_gps_ms = thisgps_millis-thispps_millis;

  // when the gps timestamp is from the last second, adjust it forward one
  if (pps_to_gps_ms > -990 && pps_to_gps_ms < -100) {
    pps_to_gps_ms += 1000;
    thisgpstime++;
  }

  compare_ptp();

  // when the gps timestamp is outside of the normal window, reject it
  // this value is typically around 300ms
  if (pps_to_gps_ms > 1000 || pps_to_gps_ms < 5) {
    write_uart_s("pps to gps out of window ");
    write_uart_i(pps_to_gps_ms);
    write_uart_s("\n");
    return;
  }

  // if we've already seen this timestamp
  if(thispps == lastpps || thisgpstime == lastgpstime) {
    return;
  }

  lastpps = thispps;
  lastgpstime = thisgpstime;

  if(!time_set) {
    uint32_t compileTime = compile.ntptime();
    struct timestamp ptp_now;

    // allow for compile timezone to be 12 hours ahead
    compileTime -= 12*60*60;

    if (thisgpstime < compileTime) {
      write_uart_s("bad gps time: ");
      write_uart_u(thisgpstime);
      write_uart_s("\n");
      return;
    }

    localClock.setTime(thispps, thisgpstime);
    ClockPID.add_sample(thispps, thisgpstime, 0);

    ptp_timestamp(&ptp_now);
    ptp_update_s(thisgpstime - ptp_now.seconds);

    time_set = true;
    write_uart_s("set clock: ");
    write_uart_u(thisgpstime);
    write_uart_s(" delay ");
    write_uart_u(pps_to_gps_ms);
    write_uart_s("\n");
  } else {
    int64_t offset = localClock.getOffset(thispps, thisgpstime, 0);
    last_local_offset = offset;

    ClockPID.add_sample(thispps, thisgpstime, offset);

    server.setReftime(thisgpstime);
    localClock.setPpb(thispps, ClockPID.out() * billion);

#ifdef DEBUG_SERIAL
    write_uart_s("offset: ");
    // convert from NTP fraction to ns
    write_uart_64i(offset * 1000000000LL / 4294967296LL);
    write_uart_s(" P=");
    write_uart_i(ClockPID.p_out() * billion);
    write_uart_s(" I=");
    write_uart_i(ClockPID.i_out() * billion);
    write_uart_s(" D=");
    write_uart_i(ClockPID.d_out() * billion);
    write_uart_s(" ppb=");
    write_uart_i(localClock.getPpb());
    write_uart_s(" T=");
    write_uart_u(thisgpstime);
    write_uart_s(" delay ");
    write_uart_u(pps_to_gps_ms);
    write_uart_s("\n");
#endif
  }
}

}
