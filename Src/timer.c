#include "main.h"

#include "timer.h"
#include "uart.h"
#include "ethernetif.h"
#include "ptp.h"
#include "GPS.h"

volatile struct pps_capture_t pps_capture;

uint32_t get_counters() {
  return __HAL_TIM_GET_COUNTER(&htim2);
}

void timer_start() {
  // runs the ADC
  HAL_TIM_Base_Start(&htim3);

  pps_capture.events = 0;
  HAL_TIM_Base_Start(&htim2);
  HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_4);
  HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
  uint32_t irq_milli, irq_time;

  irq_milli = HAL_GetTick();
  irq_time = get_counters();
  if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
    // PTP -> TIM input capture
    pps_capture.ptp_cap = HAL_TIM_ReadCapturedValue(&htim2, TIM_CHANNEL_1);
    pps_capture.ptp_irq_milli = irq_milli;
    pps_capture.ptp_irq_time = irq_time;
    pps_capture.ptp_seconds = heth.Instance->PTPTTHR;
    ptp_pending = 0;
  } else if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4) {
    // PPS input capture
    pps_capture.cap = HAL_TIM_ReadCapturedValue(&htim2, TIM_CHANNEL_4);
    pps_capture.irq_time = irq_time;
    pps_capture.irq_milli = irq_milli;
    pps_capture.events++;
  }
}

uint32_t get_last_pps() {
  return pps_capture.cap;
}

uint32_t get_last_pps_millis() {
  return pps_capture.irq_milli;
}

void print_tim() {
  write_uart_s("pps ");
  write_uart_u(gps_time());
  write_uart_s(" ");
  write_uart_u(pps_capture.cap);
  write_uart_s(" ");
  write_uart_u(pps_capture.irq_milli);
  write_uart_s(" ");
  write_uart_u(pps_capture.events);
  write_uart_s(" ptp ");
  write_uart_u(pps_capture.ptp_seconds);
  write_uart_s(" ");
  write_uart_u(pps_capture.ptp_cap);
  write_uart_s(" ");
  write_uart_u(pps_capture.ptp_irq_milli);
  write_uart_s("\n");
}
