#include "main.h"
#include <stdint.h>
#include "bme280.h"
#include "i2c.h"
#include "uart.h"

#define I2C_ADDR 0x77

// register offsets
#define BME280_ID          0xD0
#define BME280_CTRL_HUMID  0xF2
#define BME280_STATUS      0xF3
#define BME280_CTRL_MEAS   0xF4
#define BME280_CONFIG      0xF5
#define BME280_CALIBRATION_0_25 0x88
#define BME280_CALIBRATION_26_41 0xE1
struct raw_adc_data {
  uint32_t temp, pressure;
  uint16_t humidity;
};

// register offsets, adc data
#define BME280_PRESSURE    0xF7
#define BME280_TEMPERATURE 0xFA
#define BME280_HUMIDITY    0xFD

// CTRL_HUMID settings
#define CTRL_HUMID_OFF 0b000
#define CTRL_HUMID_1X  0b001
#define CTRL_HUMID_2X  0b010
#define CTRL_HUMID_4X  0b011
#define CTRL_HUMID_8X  0b100
#define CTRL_HUMID_16X 0b101

// STATUS bitflags
#define STATUS_MEASURING 0b1000
#define STATUS_B2         0b100
#define STATUS_REG_UPDATE   0b1

// CTRL_MEAS settings
#define CTRL_MEAS_TEMP_OFF 0b00000000
#define CTRL_MEAS_TEMP_1X  0b00100000
#define CTRL_MEAS_TEMP_2X  0b01000000
#define CTRL_MEAS_TEMP_4X  0b01100000
#define CTRL_MEAS_TEMP_8X  0b10000000
#define CTRL_MEAS_TEMP_16X 0b10100000
#define CTRL_MEAS_PRES_OFF    0b00000
#define CTRL_MEAS_PRES_1X     0b00100
#define CTRL_MEAS_PRES_2X     0b01000
#define CTRL_MEAS_PRES_4X     0b01100
#define CTRL_MEAS_PRES_8X     0b10000
#define CTRL_MEAS_PRES_16X    0b10100
#define CTRL_MEAS_MODE_SLEEP     0b00
#define CTRL_MEAS_MODE_FORCED    0b01
#define CTRL_MEAS_MODE_NORMAL    0b11
static const char *oversample_names[] = {"off", "1x", "2x", "4x", "8x", "16x", "16x", "16x"};
static const char *mode_names[] = {"sleep","forced","normal","normal"};

// CONFIG settings
#define CONFIG_STANDBY_0_5ms  0b00000000
#define CONFIG_STANDBY_62_5ms 0b00100000
#define CONFIG_STANDBY_125ms  0b01000000
#define CONFIG_STANDBY_250ms  0b01100000
#define CONFIG_STANDBY_500ms  0b10000000
#define CONFIG_STANDBY_1000ms 0b10100000
#define CONFIG_STANDBY_10ms   0b11000000
#define CONFIG_STANDBY_20ms   0b11100000
#define CONFIG_IIR_FILTER_OFF    0b00000
#define CONFIG_IIR_FILTER_2      0b00100
#define CONFIG_IIR_FILTER_4      0b01000
#define CONFIG_IIR_FILTER_8      0b01100
#define CONFIG_IIR_FILTER_16     0b10000
#define CONFIG_SPI3W                 0b1
static const char *standby_names[] = {"0.5ms","62.5ms","125ms","250ms","500ms","1s","10ms","20ms"};
static const char *iir_filter_names[] = {"off","2","4","8","16","16","16","16"};

void bme280_read_calibration(struct bme280_calibration_data *c) {
  uint8_t calibration_0_25[26];
  uint8_t calibration_26_41[7]; // more data available here, but only 7 bytes are used

  i2c_read_register(I2C_ADDR, BME280_CALIBRATION_0_25, sizeof(calibration_0_25), (uint8_t *)&calibration_0_25);
  i2c_read_register(I2C_ADDR, BME280_CALIBRATION_26_41, sizeof(calibration_26_41), (uint8_t *)&calibration_26_41);

  c->T1 = calibration_0_25[0] | (calibration_0_25[1] << 8);
  c->T2 = calibration_0_25[2] | (calibration_0_25[3] << 8);
  c->T3 = calibration_0_25[4] | (calibration_0_25[5] << 8);

  c->P1 = calibration_0_25[6] | (calibration_0_25[7] << 8);
  for(uint8_t i = 0; i < 8; i++) {
    c->P2_9[i] = calibration_0_25[8+2*i] | (calibration_0_25[9+2*i] << 8);
  }

  c->H1 = calibration_0_25[25];
  c->H2 = calibration_26_41[0] | (calibration_26_41[1] << 8);
  c->H3 = calibration_26_41[2];
  c->H4 = (calibration_26_41[3] << 4) | (calibration_26_41[4] & 0b1111);
  c->H5 = ((calibration_26_41[4] & 0b11110000) >> 4) | (calibration_26_41[5] << 4);
  c->H6 = calibration_26_41[6];
}

static void read_raw_temp_adc(struct raw_adc_data *r) {
  uint8_t d[3];

  i2c_read_register(I2C_ADDR, BME280_TEMPERATURE, sizeof(d), (uint8_t *)&d);

  r->temp = (d[2] >> 4) | (d[1] << 4) | (d[0] << 12);
}

static void read_raw_adc(struct raw_adc_data *r) {
  uint8_t d[8];

  i2c_read_register(I2C_ADDR, BME280_PRESSURE, sizeof(d), (uint8_t *)&d);

  r->pressure = (d[2] >> 4) | (d[1] << 4) | (d[0] << 12);
  r->temp = (d[5] >> 4) | (d[4] << 4) | (d[3] << 12);
  r->humidity = (d[6] << 8) | d[7];
}

static int32_t t_fine; // needed in calc_pressure and calc_humidity
// returns C, from BME280 datasheeet
static float calc_temp(const struct raw_adc_data *r, const struct bme280_calibration_data *c) {
  int32_t var1, var2;
  float C;

  var1  = ((((r->temp>>3) - ((int32_t)c->T1<<1))) * ((int32_t)c->T2)) >> 11;
  var2  = (((((r->temp>>4) - ((int32_t)c->T1)) * ((r->temp>>4) - ((int32_t)c->T1))) >> 12) * ((int32_t)c->T3)) >> 14;
  t_fine = var1 + var2;
  C = (t_fine * 5 + 128) / 25600.0;
  return C;
}

// returns hPa, requires t_fine set (call calc_temp), from BME280 datasheet
static float calc_pressure(const struct raw_adc_data *r, const struct bme280_calibration_data *c) {
  int64_t var1, var2, p;

  var1 = ((int64_t)t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)c->P2_9[4];
  var2 = var2 + ((var1*(int64_t)c->P2_9[3])<<17);
  var2 = var2 + (((int64_t)c->P2_9[2])<<35);
  var1 = ((var1 * var1 * (int64_t)c->P2_9[1])>>8) + ((var1 * (int64_t)c->P2_9[0])<<12);
  var1 = (((((int64_t)1)<<47)+var1))*((int64_t)c->P1)>>33;
  if (var1 == 0) {
    return 0; // avoid exception caused by division by zero
  }
  p = 1048576 - r->pressure;
  p = (((p<<31)-var2)*3125)/var1;
  var1 = (((int64_t)c->P2_9[7]) * (p>>13) * (p>>13)) >> 25;
  var2 = (((int64_t)c->P2_9[6]) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + (((int64_t)c->P2_9[5])<<4);

  return p / 25600.0;
}

// returns %RH, requires t_fine set (call calc_temp), from BME280 datasheet
static float calc_humidity(const struct raw_adc_data *r, const struct bme280_calibration_data *c) {
  int32_t v_x1_u32r;

  v_x1_u32r = (t_fine - ((int32_t)76800));
  v_x1_u32r =
    (
      ((
        ((r->humidity << 14) - (((int32_t)c->H4) << 20) - (((int32_t)c->H5) * v_x1_u32r)) + ((int32_t)16384)
      ) >> 15) *
      (((
        ((((v_x1_u32r * ((int32_t)c->H6)) >> 10) * (((v_x1_u32r * ((int32_t)c->H3)) >> 11) + (( int32_t)32768))) >> 10) +
        ((int32_t)2097152)) * ((int32_t)c->H2) + 8192) >> 14
      )
    );
  v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)c->H1)) >> 4));
  v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r); 
  v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
  return v_x1_u32r / 4194304.0;
}

uint8_t bme280_getid() {
  uint8_t r = 0;
  if(i2c_read_register(I2C_ADDR, BME280_ID, sizeof(r), &r) != HAL_OK) {
    write_uart_s("read ID failed\n");
  }
  return r;
}

void bme280_show() {
  uint8_t d[4];
  i2c_read_register(I2C_ADDR, BME280_CTRL_HUMID, sizeof(d), (uint8_t *)&d);

  write_uart_s("ctrl_humid = ");
  write_uart_u(d[0]);
  write_uart_s(" (h=");
  write_uart_s(oversample_names[d[0] & 0b111]);
  write_uart_s(")\nstatus = ");
  write_uart_u(d[1]);
  write_uart_s((d[1] & STATUS_MEASURING) ? " measuring" : "");
  write_uart_s((d[1] & STATUS_B2) ? " +b2" : "");
  write_uart_s((d[1] & STATUS_REG_UPDATE) ? " regupdate" : "");
  write_uart_s("\nctrl_meas = ");
  write_uart_u(d[2]);
  write_uart_s(" (t=");
  write_uart_s(oversample_names[(d[2] & 0b11100000) >> 5]);
  write_uart_s(", p=");
  write_uart_s(oversample_names[(d[2] &    0b11100) >> 2]);
  write_uart_s(", m=");
  write_uart_s(mode_names[       d[2] &       0b11]);
  write_uart_s(")\nconfig = ");
  write_uart_u(d[3]);
  write_uart_s(" (standby=");
  write_uart_s(standby_names[(d[3] & 0b11100000) >> 5]);
  write_uart_s(", iir=");
  write_uart_s(iir_filter_names[(d[3] & 0b11100) >> 2]);
  write_uart_s(", spi=");
  write_uart_s((d[3] & CONFIG_SPI3W) ? "3w" : "n");
  write_uart_s(")\n");
}

void bme280_raw() {
  struct raw_adc_data r;
  read_raw_adc(&r);
  write_uart_s("P = ");
  write_uart_u(r.pressure);
  write_uart_s(" T = ");
  write_uart_u(r.temp);
  write_uart_s(" H = ");
  write_uart_u(r.humidity);
  write_uart_s("\n");
}

void bme280_printcal() {
  struct bme280_calibration_data c;
  bme280_read_calibration(&c);

  write_uart_s("T1,T2,T3 ");
  write_uart_u(c.T1);
  write_uart_s(", ");
  write_uart_u(c.T2);
  write_uart_s(", ");
  write_uart_u(c.T3);
  write_uart_s("\nP1,P2,P3 ");
  write_uart_u(c.P1);
  write_uart_s(", ");
  write_uart_u(c.P2_9[0]);
  write_uart_s(", ");
  write_uart_u(c.P2_9[1]);
  write_uart_s("\nP4,P5,P6 ");
  write_uart_u(c.P2_9[2]);
  write_uart_s(", ");
  write_uart_u(c.P2_9[3]);
  write_uart_s(", ");
  write_uart_u(c.P2_9[4]);
  write_uart_s("\nP7,P8,P9 ");
  write_uart_u(c.P2_9[5]);
  write_uart_s(", ");
  write_uart_u(c.P2_9[6]);
  write_uart_s(", ");
  write_uart_u(c.P2_9[7]);
  write_uart_s("\nH1,H2,H3 ");
  write_uart_u(c.H1);
  write_uart_s(", ");
  write_uart_u(c.H2);
  write_uart_s(", ");
  write_uart_u(c.H3);
  write_uart_s("\nH4,H5,H6 ");
  write_uart_u(c.H4);
  write_uart_s(", ");
  write_uart_u(c.H5);
  write_uart_s(", ");
  write_uart_u(c.H6);
  write_uart_s("\n");
}

void bme280_force_mode() {
  i2c_write_register(I2C_ADDR, BME280_CTRL_HUMID, CTRL_HUMID_1X);
  i2c_write_register(I2C_ADDR, BME280_CTRL_MEAS, CTRL_MEAS_TEMP_1X | CTRL_MEAS_PRES_1X | CTRL_MEAS_MODE_FORCED);
}

void bme280_normal_mode() {
  i2c_write_register(I2C_ADDR, BME280_CTRL_HUMID, CTRL_HUMID_1X);
  i2c_write_register(I2C_ADDR, BME280_CTRL_MEAS, CTRL_MEAS_TEMP_1X | CTRL_MEAS_PRES_1X | CTRL_MEAS_MODE_NORMAL);
  i2c_write_register(I2C_ADDR, BME280_CONFIG, CONFIG_STANDBY_500ms | CONFIG_IIR_FILTER_OFF);
}

int32_t bme280_curtemp(const struct bme280_calibration_data *c) {
  struct raw_adc_data r;
  float C, F;

  read_raw_temp_adc(&r);

  C = calc_temp(&r, c);
  F = C * 9 / 5 + 32;
  return F*1000;
}

void bme280_print(const struct bme280_calibration_data *c) {
  struct raw_adc_data r;
  float C, F, pressure, RH;
  uint8_t status;

  i2c_read_register(I2C_ADDR, BME280_STATUS, sizeof(status), &status);

  write_uart_s("status = ");
  write_uart_u(status);
  write_uart_s("\n");

  read_raw_adc(&r);

  C = calc_temp(&r, c);
  F = C * 9 / 5 + 32;
  write_uart_s("T = ");
  write_uart_i(C*1000);
  write_uart_s(" mC ");
  write_uart_i(F*1000);
  write_uart_s(" mF (raw=");
  write_uart_u(r.temp);
  write_uart_s(")\n");

  pressure = calc_pressure(&r, c);
  write_uart_s("P = ");
  write_uart_i(pressure*1000);
  write_uart_s("mhPa (raw=");
  write_uart_u(r.pressure);
  write_uart_s(")\n");

  RH = calc_humidity(&r, c);
  write_uart_s("H = ");
  write_uart_i(RH*1000);
  write_uart_s("/100000 RH (raw=");
  write_uart_u(r.humidity);
  write_uart_s(")\n");
}
