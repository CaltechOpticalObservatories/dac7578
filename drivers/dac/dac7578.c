/**
 * @file Driver for DAC7578 I2C-based 8-Channel DAC.
 */


#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/dac.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/logging/log.h>

#include "drivers/dac/dac7578.h"

#define LOG_LEVEL CONFIG_GPIO_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(i2c_dac7578);

#define DT_DRV_COMPAT ti_dac7578


struct dac7578_config {
	struct i2c_dt_spec bus;
	uint8_t resolution;
};

struct dac7578_data {
	uint8_t configured;
};


static int dac7578_reg_read(const struct device *dev, uint8_t reg,
			      uint16_t *val)
{
	const struct dac7578_config *cfg = dev->config;

	if (i2c_burst_read_dt(&cfg->bus, reg, (uint8_t *) val, 2) < 0) {
		LOG_ERR("I2C read failed");
		return -EIO;
	}

	*val = sys_be16_to_cpu(*val);

	return 0;
}

static int dac7578_reg_write(const struct device *dev, uint8_t control_word,
			       uint16_t val)
{
	const struct dac7578_config *cfg = dev->config;
	uint8_t buf[3];

	buf[0] = control_word;

	buf[1] = (val >> 4) & 0xFF;
	buf[2] = (val & 0x0F) << 4;

	return i2c_write_dt(&cfg->bus, buf, sizeof(buf));
}


static int __maybe_unused dac7578_reg_update(const struct device *dev, uint8_t reg,
					     uint16_t mask, bool setting)
{
	uint16_t regval;
	int ret;

	ret = dac7578_reg_read(dev, reg, &regval);
	if (ret) {
		return -EIO;
	}

	if (setting) {
		regval |= mask;
	} else {
		regval &= ~mask;
	}

	ret = dac7578_reg_write(dev, reg, regval);
	if (ret) {
		return ret;
	}

	return 0;
}

static int dac7578_channel_setup(const struct device *dev,
				   const struct dac_channel_cfg *channel_cfg)
{
	const struct dac7578_config *config = dev->config;
	struct dac7578_data *data = dev->data;
	uint8_t control_word;
	int ret;

	if (channel_cfg->channel_id > DAC7578_MAX_CHANNEL - 1) {
		LOG_ERR("Unsupported channel %d", channel_cfg->channel_id);
		return -ENOTSUP;
	}

	if (channel_cfg->resolution != config->resolution) {
		LOG_ERR("Unsupported resolution %d", channel_cfg->resolution);
		return -ENOTSUP;
	}

	// if (channel_cfg->internal) {
	// 	LOG_ERR("Internal channels not supported");
	// 	return -ENOTSUP;
	// }

	if (data->configured & BIT(channel_cfg->channel_id)) {
		LOG_DBG("Channel %d already configured", channel_cfg->channel_id);
		return 0;
	}

	control_word = (0x00 << 4) | (channel_cfg->channel_id & 0x07);

	ret = dac7578_reg_write(dev, control_word, 0);
	if (ret) {
		LOG_ERR("Unable to power up channel %d", channel_cfg->channel_id);
		return -EIO;
	}

	data->configured |= BIT(channel_cfg->channel_id);

	LOG_DBG("Channel %d initialized", channel_cfg->channel_id);

	return 0;
}


static int dac7578_write_value(const struct device *dev, uint8_t channel,
				uint32_t value)
{
	const struct dac7578_config *config = dev->config;
	struct dac7578_data *data = dev->data;
    uint8_t control_word;
	uint16_t regval;
	int ret;

	if (channel > DAC7578_MAX_CHANNEL - 1) {
		LOG_ERR("Unsupported channel %d", channel);
		return -ENOTSUP;
	}

	if (!(data->configured & BIT(channel))) {
		LOG_ERR("Channel %d not initialized", channel);
		return -EINVAL;
	}

	if (value >= (1 << (config->resolution))) {
		LOG_ERR("Value %d out of range", value);
		return -EINVAL;
	}

    control_word = (channel & 0x0F); //input register of channel

	regval = (value & 0x0FFF);

	ret = dac7578_reg_write(dev, control_word, regval);
	if (ret) {
		LOG_ERR("Unable to set value %d on channel %d", value, channel);
		return -EIO;
	}

	return 0;
}

int dac7578_read_value(const struct device *dev, uint8_t channel,
		       uint32_t *value)
{
	struct dac7578_data *data = dev->data;
	uint8_t control_word;
	uint16_t regval;
	int ret;

	if (channel > DAC7578_MAX_CHANNEL - 1) {
		LOG_ERR("Unsupported channel %d", channel);
		return -ENOTSUP;
	}

	if (!(data->configured & BIT(channel))) {
		LOG_ERR("Channel %d not initialized", channel);
		return -EINVAL;
	}

	control_word = (channel&0x0F) + 0x10; //DAC register, not the input register
	ret = dac7578_reg_read(dev, control_word, &regval);
	if (ret) {
		LOG_ERR("I2C read value failed");
		return -EIO;
	}

	regval = sys_be16_to_cpu(regval);
	*value = (regval >> 4) & 0x0FFF;

	return 0;
}

static int dac7578_soft_reset(const struct device *dev)
{
    uint8_t control_word; 
	int ret;

    control_word = (0x07 << 4); //soft reset

	// Send the software reset command (no value to write)
	ret = dac7578_reg_write(dev, control_word, 0);
	if (ret) {
		LOG_ERR("Software reset failed");
		return -EIO;
	}

	// Wait for the reset to complete
	k_msleep(DAC7578_POR_DELAY);

	return 0;
}


static int dac7578_init(const struct device *dev)
{
	const struct dac7578_config *config = dev->config;
	struct dac7578_data *data = dev->data;
	int ret;

	if (!device_is_ready(config->bus.bus)) {
		LOG_ERR("I2C device not ready");
		return -ENODEV;
	}

	ret = dac7578_soft_reset(dev);
	if (ret) {
		LOG_ERR("Soft-reset failed");
		return ret;
	}

	data->configured = 0;

	LOG_DBG("Init complete");

	return 0;
}


static const struct dac_driver_api dac7578_driver_api = {
	.channel_setup =  dac7578_channel_setup,
	.write_value =  dac7578_write_value
};


#define INST_DT_DAC7578(inst) DT_INST(inst, ti_dac7578)

#define DAC7578_DEVICE(n) \
	static struct dac7578_data dac7578_data_##n; \
	static const struct dac7578_config dac7578_config_##n = { \
		.bus = I2C_DT_SPEC_GET(INST_DT_DAC7578(n)), \
		.resolution = 12, \
	}; \
	DEVICE_DT_DEFINE(INST_DT_DAC7578(n), \
				&dac7578_init, NULL, \
				&dac7578_data_##n, \
				&dac7578_config_##n, POST_KERNEL, \
				CONFIG_DAC7578_INIT_PRIORITY, \
				&dac7578_driver_api)

/* Define the instantiation macro */
#define CALL_WITH_ARG(arg, expr) expr(arg)

#define INST_DT_DAC7578_FOREACH(inst_expr) \
	LISTIFY(DT_NUM_INST_STATUS_OKAY(ti_dac7578), \
		     CALL_WITH_ARG, (), inst_expr)

/* Instantiate all DAC7578 devices */
INST_DT_DAC7578_FOREACH(DAC7578_DEVICE);
