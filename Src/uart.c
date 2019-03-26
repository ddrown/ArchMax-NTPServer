#include "main.h"

#include <string.h>
#include <stdlib.h>

#include "uart.h"
#include "int64.h"

void write_uart_ch(char ch) {
  HAL_UART_Transmit(&UART_NAME, (uint8_t *)&ch, 1, 500);
}

void write_uart_s(const char *s) {
  HAL_UART_Transmit(&UART_NAME, (uint8_t *)s, strlen(s), 500);
}

void write_uart_u(uint32_t i) {
  char buffer[12];
  utoa(i, buffer, 10);
  write_uart_s(buffer);
}

void write_uart_i(int32_t i) {
  char buffer[12];
  itoa(i, buffer, 10);
  write_uart_s(buffer);
}

void write_uart_64i(int64_t i) {
  char buffer[21];
  i64toa(i, buffer);
  write_uart_s(buffer);
}

void write_uart_64u(uint64_t i) {
  char buffer[21];
  u64toa(i, buffer);
  write_uart_s(buffer);
}

void write_uart_hex(uint8_t i) {
  char buffer[3];
  utoa(i, buffer, 16);
  if(buffer[1] == '\0') {
    buffer[2] = '\0';
    buffer[1] = buffer[0];
    buffer[0] = '0';
  }
  write_uart_s(buffer);
}

uint8_t uartData;
char uartBuffer[10];
uint8_t start = 0, end = 0, overrun = 0;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  uint8_t newEnd = (end + 1) % sizeof(uartBuffer);
  if(newEnd == start) { // no space left in uartBuffer
    overrun = 1;
  } else {
    end = newEnd;
    uartBuffer[end] = uartData;
  }
  HAL_UART_Receive_IT(huart, &uartData, 1); // TODO: is there a race condition here?
}

void start_rx_uart() {
  HAL_UART_Receive_IT(&UART_NAME, &uartData, 1);
}

int8_t uart_rx_ready() {
  if(overrun) { // software buffer overrun
    overrun = 0;
  }

#ifdef UART_CLEAR_OREF
  // clear overflow flag
  if(__HAL_UART_GET_FLAG(&UART_NAME, UART_CLEAR_OREF) != RESET) { // hardware buffer overrun
    __HAL_UART_CLEAR_FLAG(&UART_NAME, UART_CLEAR_OREF);
  }
#endif

  HAL_UART_Receive_IT(&UART_NAME, &uartData, 1);
  return start != end;
}

char uart_rx_data() {
  if(start == end) {
    return '\0';
  }
  start = (start + 1) % sizeof(uartBuffer);
  return uartBuffer[start];
}
