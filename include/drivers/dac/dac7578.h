/*
 * Copyright (c) 2024 FKMG Circuits
 */

#ifndef ZEPHYR_DRIVERS_DAC_DAC7578_H_
#define ZEPHYR_DRIVERS_DAC_DAC7578_H_

#include <zephyr/kernel.h>
#include <zephyr/drivers/dac.h>
#include <zephyr/drivers/i2c.h>

#define DAC7578_POR_DELAY   5
#define DAC7578_MAX_CHANNEL 8

int dac7578_read_value(const struct device *dev, uint8_t channel,
		       uint32_t *value);

#endif /* ZEPHYR_DRIVERS_DAC_DAC7578_H_ */
