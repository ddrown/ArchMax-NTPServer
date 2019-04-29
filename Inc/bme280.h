#ifndef BME280_H
#define BME280_H

struct bme280_calibration_data {
  uint16_t T1;
  int16_t T2;
  int16_t T3;
  uint16_t P1;
  int16_t P2_9[8];
  uint8_t H1;
  int16_t H2;
  uint8_t H3;
  int16_t H4;
  int16_t H5;
  int8_t H6;
};

uint8_t bme280_getid();
void bme280_read_calibration(struct bme280_calibration_data *c);
void bme280_printcal();
void bme280_force_mode();
void bme280_normal_mode();
void bme280_print(const struct bme280_calibration_data *c);
void bme280_raw();
void bme280_show();
int32_t bme280_curtemp(const struct bme280_calibration_data *c);

#endif
