/*
 * Copyright (c) 2024 FKMG Circuits
 */

#ifndef ZEPHYR_DRIVERS_DAC_DAC7578_H_
#define ZEPHYR_DRIVERS_DAC_DAC7578_H_

#include <drivers/dac/dac7x78.h>

/*
 * Compatibility wrapper for code written against the original DAC7578-only
 * fork. New code should include dac7x78.h and call dac7x78_read_value().
 */
#define DAC7578_POR_DELAY DAC7X78_RESET_DELAY_MS
#define DAC7578_MAX_CHANNEL DAC7X78_CHANNEL_COUNT

static inline int dac7578_read_value(const struct device *dev, uint8_t channel,
				     uint32_t *value)
{
	return dac7x78_read_value(dev, channel, value);
}

#endif /* ZEPHYR_DRIVERS_DAC_DAC7578_H_ */
