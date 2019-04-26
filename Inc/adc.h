#ifndef ADC_H
#define ADC_H

extern ADC_HandleTypeDef hadc1;
void adc_poll();
void update_adc();
void print_last_vcc();
void print_last_temp();
int32_t last_temp();
void print_adc();
void adc_init();

#endif
