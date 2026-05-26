/*
 * Copyright (c) 2024 FKMG Circuits
 */

#ifndef ZEPHYR_DRIVERS_DAC_DAC7X78_H_
#define ZEPHYR_DRIVERS_DAC_DAC7X78_H_

#include <zephyr/kernel.h>
#include <zephyr/drivers/dac.h>
#include <zephyr/drivers/i2c.h>

#define DAC7X78_RESET_DELAY_MS 5
#define DAC7X78_CHANNEL_COUNT 8

int dac7x78_read_value(const struct device *dev, uint8_t channel,
		       uint32_t *value);

#endif /* ZEPHYR_DRIVERS_DAC_DAC7X78_H_ */
