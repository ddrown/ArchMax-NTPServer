#ifndef TIMER_H
#define TIMER_H

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;

uint32_t get_counters();
void timer_start();
void print_tim();

extern volatile struct pps_capture_t {
  uint32_t irq_milli;
  uint32_t irq_time;
  uint32_t cap;
  uint32_t ptp_seconds;
  uint32_t ptp_irq_milli;
  uint32_t ptp_irq_time;
  uint32_t ptp_cap;
  uint8_t events;
} pps_capture;

#endif
