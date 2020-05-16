extern "C" {
#include "main.h"
#include "GPS.h"
#include "uart.h"
}

#include "DateTime.h"
#include "GPSDateTime.h"

static GPSDateTime gps;

extern "C" {
static uint8_t ch;
static uint32_t charcount = 0;
static bool hasValidTime = false;

void start_rx_gps() {
  HAL_UART_Receive_IT(&GPS_UART_NAME, &ch, 1);
}

void rx_gps() {
  charcount++;
  if(gps.addch(ch)) {
    hasValidTime = true;
  }
  HAL_UART_Receive_IT(&GPS_UART_NAME, &ch, 1);
}

void gpstime() {
  write_uart_s("chars: ");
  write_uart_u(charcount);
  write_uart_s(" ");
  if(hasValidTime) {
    write_uart_s("time: ");
    write_uart_u(gps.GPSnow().ntptime());
    write_uart_s("\n");
  } else {
    write_uart_s("no time yet\n");
  }
}

uint32_t gps_time() {
  if(!hasValidTime) {
    return 0;
  }
  return gps.GPSnow().ntptime();
}

uint32_t gps_time_millis() {
  return gps.capturedAt();
}

}
