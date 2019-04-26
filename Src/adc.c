#include "main.h"
#include <stdint.h>
#include "adc.h"
#include "uart.h"
#include "stm32f4xx_ll_adc.h"
#include "ptp.h"

// 3.3V per count of 4096 in nV
#define NV_PER_COUNT 805664
#define NV_PER_MV 1000000

#define AVERAGE_SAMPLES_ADC 10

static uint16_t internal_temps[AVERAGE_SAMPLES_ADC];
static uint16_t internal_vrefs[AVERAGE_SAMPLES_ADC];
static uint16_t external_temps[AVERAGE_SAMPLES_ADC];
static int8_t adc_index = -1;

#define AVERAGE_SAMPLES_CALC 10

static int32_t temps[AVERAGE_SAMPLES_CALC];
static int32_t ext_temps[AVERAGE_SAMPLES_CALC];
static uint64_t vccs[AVERAGE_SAMPLES_CALC];
static int8_t calc_index = -1;

static uint16_t *ts_cal1 = TEMPSENSOR_CAL1_ADDR;
static uint16_t *ts_cal2 = TEMPSENSOR_CAL2_ADDR;
static uint16_t *vrefint_cal = VREFINT_CAL_ADDR;

static int32_t last_temp_mF = 0;
static uint64_t last_vcc_nv = 0;
static int32_t last_ext_temp = 0;

#define ADC_CHANNELS 5
static volatile uint16_t adc_buffer[ADC_CHANNELS];

void adc_init() {
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_buffer, ADC_CHANNELS);
}

static uint16_t avg_16(uint16_t *values, uint8_t length) {
  uint32_t sum = 0;
  for(uint8_t i = 0; i < length; i++) {
    sum += values[i];
  }
  return sum / length;
}

static uint64_t avg_64(uint64_t *values, uint8_t length) {
  uint64_t sum = 0;
  for(uint8_t i = 0; i < length; i++) {
    sum += values[i];
  }
  return sum / length;
}

static int32_t avg_i(int32_t *values, uint8_t length) {
  int64_t sum = 0;
  for(uint8_t i = 0; i < length; i++) {
    sum += values[i];
  }
  return sum / length;
}

void print_adc() {
  struct timestamp now;

  ptp_timestamp(&now);
  write_uart_u(now.seconds);
  write_uart_s(" ");
  write_uart_u(last_temp_mF);
  write_uart_s(" ");
  write_uart_u(last_vcc_nv/1000000);
  write_uart_s(" ");
  write_uart_u(internal_vrefs[adc_index]);
  write_uart_s(" ");
  write_uart_u(external_temps[adc_index]);
  write_uart_s(" ");
  write_uart_u(last_ext_temp/1000);
  write_uart_s("\n");
}

void adc_poll() {
  if(adc_index == -1) {
    adc_index++;
    for(uint8_t i = 0; i < AVERAGE_SAMPLES_ADC; i++) {
      internal_temps[i] = adc_buffer[0];
      internal_vrefs[i] = adc_buffer[1];
      external_temps[i] = adc_buffer[2];
    }
  } else {
    adc_index = (adc_index + 1) % AVERAGE_SAMPLES_ADC;
    internal_temps[adc_index] = adc_buffer[0];
    internal_vrefs[adc_index] = adc_buffer[1];
    external_temps[adc_index] = adc_buffer[2];
  }
}

void print_last_temp() {
  write_uart_s("temp ");
  write_uart_u(last_temp_mF);
  write_uart_s(" mF\n");
}

int32_t last_temp() {
  return last_temp_mF;
}

void print_last_vcc() {
  write_uart_s("vcc ");
  write_uart_u(last_vcc_nv/1000000);
  write_uart_s(" mV\n");
}

void update_adc() {
  uint8_t set_all = 0;
  uint16_t temp = avg_16(internal_temps, AVERAGE_SAMPLES_ADC);
  uint16_t vref = avg_16(internal_vrefs, AVERAGE_SAMPLES_ADC);
  uint16_t ext_temp = avg_16(external_temps, AVERAGE_SAMPLES_ADC);

  if(calc_index == -1) {
    calc_index++;
    set_all = 1;
  } else {
    calc_index = (calc_index + 1) % AVERAGE_SAMPLES_CALC;
  }

  uint32_t vref_expected_nv = (*vrefint_cal) * NV_PER_COUNT;
  uint32_t vref_actual_nv = vref*NV_PER_COUNT;
  uint64_t vcc_nv = (uint64_t)((vref_expected_nv+500)/1000)*VREFINT_CAL_VREF*NV_PER_MV/((vref_actual_nv+500)/1000);

  if(set_all) {
    for(uint8_t i = 0; i < AVERAGE_SAMPLES_CALC; i++) {
      vccs[i] = vcc_nv;
    }
  } else {
    vccs[calc_index] = vcc_nv;
  }
  last_vcc_nv = avg_64(vccs, AVERAGE_SAMPLES_CALC);

  uint32_t nv_per_count = last_vcc_nv / 4096;

  uint32_t temp_nv = temp * nv_per_count;
  uint32_t nv_30C = (*ts_cal1) * NV_PER_COUNT;
  uint32_t nv_110C = (*ts_cal2) * NV_PER_COUNT;

  int32_t uv_per_C = (((int32_t)nv_110C - (int32_t)nv_30C) / (TEMPSENSOR_CAL2_TEMP-TEMPSENSOR_CAL1_TEMP) + 500)/1000;
  int32_t temp_mC = ((int32_t)temp_nv - (int32_t)nv_30C) / uv_per_C + 30000;
  int32_t temp_mF = temp_mC * 9/5+32000;
  if(set_all) {
    for(uint8_t i = 0; i < AVERAGE_SAMPLES_CALC; i++) {
      temps[i] = temp_mF;
    }
  } else {
    temps[calc_index] = temp_mF;
  }
  last_temp_mF = avg_i(temps, AVERAGE_SAMPLES_CALC);

  uint32_t ext_temp_uv = (ext_temp * nv_per_count) / 1000;
  if(set_all) {
    for(uint8_t i = 0; i < AVERAGE_SAMPLES_CALC; i++) {
      ext_temps[i] = ext_temp_uv;
    }
  } else {
    ext_temps[calc_index] = ext_temp_uv;
  }
  last_ext_temp = avg_i(ext_temps, AVERAGE_SAMPLES_CALC);
}
