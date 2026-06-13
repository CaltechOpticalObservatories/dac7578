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

enum dac7x78_power_state {
	DAC7X78_POWER_NORMAL = 0,
	DAC7X78_POWER_DOWN_1K,
	DAC7X78_POWER_DOWN_100K,
	DAC7X78_POWER_DOWN_HIGH_Z,
};

struct dac7x78_transfer {
	uint32_t ideal_full_scale_uv;
	uint32_t output_limit_uv;
};

int dac7x78_read_value(const struct device *dev, uint8_t channel,
		       uint32_t *value);
int dac7x78_set_power_state(const struct device *dev, uint8_t channel_mask,
			    enum dac7x78_power_state state);
int dac7x78_get_transfer(const struct device *dev,
			 struct dac7x78_transfer *transfer);

#endif /* ZEPHYR_DRIVERS_DAC_DAC7X78_H_ */
