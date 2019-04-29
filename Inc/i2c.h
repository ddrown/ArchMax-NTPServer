#ifndef I2C_H
#define I2C_H

HAL_StatusTypeDef i2c_read_register(uint8_t i2c_addr, uint8_t reg_addr, uint16_t size, uint8_t *data);
HAL_StatusTypeDef i2c_write_register(uint8_t i2c_addr, uint8_t reg_addr, uint8_t value);

#endif
