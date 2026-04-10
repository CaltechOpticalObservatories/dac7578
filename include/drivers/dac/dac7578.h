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

static int dac7578_reg_read(const struct device *dev, 
                            uint8_t reg,
			                uint16_t *val);

static int dac7578_reg_write(const struct device *dev, 
                             uint8_t control_word,
			                 uint16_t val);

static int dac7578_reg_update(const struct device *dev,
                       uint8_t reg,
			           uint16_t mask, 
                       bool setting);

static int dac7578_channel_setup(const struct device *dev,
				                 const struct dac_channel_cfg *channel_cfg);

static int dac7578_write_value(const struct device *dev, 
                               uint8_t channel,
				               uint32_t value);        

static int dac7578_soft_reset(const struct device *dev);        

static int dac7578_init(const struct device *dev);

#endif /* ZEPHYR_DRIVERS_DAC_DAC7578_H_ */