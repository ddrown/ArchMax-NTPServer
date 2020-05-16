extern "C" {
#include "main.h"
#include "uart.h"
#include "timer.h"
#include "ptp.h"
}

#include "NTPClock.h"
#include "NTPClockCMD.h"

NTPClock localClock;

extern "C" {
void setntp() {
  struct timestamp now;

  ptp_timestamp(&now);

  localClock.setTime(get_last_pps(), now.seconds);
}

void getntp() {
  uint32_t sec, subsec;
  uint32_t start, end;

  start = get_counters();
  localClock.getTime(start, &sec, &subsec);
  end = get_counters();
  write_uart_u(sec);
  write_uart_s(" ");
  write_uart_u(subsec);
  write_uart_s(" ");
  write_uart_u(end-start);
  write_uart_s("\n");
}

void ntpoffset() {
  struct timestamp now;

  ptp_timestamp(&now);

  int64_t offset = localClock.getOffset(get_last_pps(), now.seconds, 0);
  write_uart_64i(offset);
  write_uart_s("\n");
}

};
