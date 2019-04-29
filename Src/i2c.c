#include "main.h"
#include "i2c.h"

extern I2C_HandleTypeDef hi2c1;

// 3ms = 150 bytes
#define I2C_TIMEOUT 3

HAL_StatusTypeDef i2c_read_register(uint8_t i2c_addr, uint8_t reg_addr, uint16_t size, uint8_t *data) {
  HAL_StatusTypeDef status;

  i2c_addr = i2c_addr << 1; // hardware expects left-aligned 7 bit addresses

  status = HAL_I2C_Master_Transmit(&hi2c1, i2c_addr, &reg_addr, sizeof(reg_addr), I2C_TIMEOUT);
  if(status != HAL_OK) {
    return status;
  }
  status = HAL_I2C_Master_Receive(&hi2c1, i2c_addr, data, size, I2C_TIMEOUT);
  return status;
}

HAL_StatusTypeDef i2c_write_register(uint8_t i2c_addr, uint8_t reg_addr, uint8_t value) {
  uint8_t data[2] = {reg_addr, value};

  i2c_addr = i2c_addr << 1; // hardware expects left-aligned 7 bit addresses

  return HAL_I2C_Master_Transmit(&hi2c1, i2c_addr, (uint8_t *)&data, sizeof(data), I2C_TIMEOUT);
}
