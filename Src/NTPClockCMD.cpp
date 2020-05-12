extern "C" {
#include "main.h"
#include "uart.h"
#include "timer.h"
#include "NTPClockCMD.h"
#include "ptp.h"
}

#include "NTPClock.h"

static NTPClock clock;

extern "C" {
void setntp() {
  struct timestamp now;

  ptp_timestamp(&now);

  clock.setTime(get_last_pps(), now.seconds);
}

void getntp() {
  uint32_t sec, subsec;
  uint32_t start, end;

  start = get_counters();
  clock.getTime(start, &sec, &subsec);
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

  int64_t offset = clock.getOffset(get_last_pps(), now.seconds, 0);
  write_uart_64i(offset);
  write_uart_s("\n");
}

};
