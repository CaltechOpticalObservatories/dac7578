/**
 * @file Driver for DAC7578/DAC7678 I2C-based 8-channel DACs.
 */


#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/dac.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/logging/log.h>

#include "drivers/dac/dac7x78.h"

LOG_MODULE_REGISTER(dac7x78, CONFIG_DAC_LOG_LEVEL);

enum dac7x78_clear_mode {
	DAC7X78_CLEAR_DEFAULT,
	DAC7X78_CLEAR_ZERO_SCALE,
	DAC7X78_CLEAR_MIDSCALE,
	DAC7X78_CLEAR_FULL_SCALE,
	DAC7X78_CLEAR_DISABLED,
};

enum dac7678_reference_mode {
	DAC7678_REFERENCE_EXTERNAL,
	DAC7678_REFERENCE_INTERNAL_STATIC,
	DAC7678_REFERENCE_INTERNAL_FLEXIBLE,
};

enum dac7x78_model {
	DAC7X78_MODEL_DAC7578,
	DAC7X78_MODEL_DAC7678,
};

struct dac7x78_config {
	struct i2c_dt_spec bus;
	uint8_t resolution;
	enum dac7x78_model model;
	enum dac7x78_clear_mode clear_mode;
	enum dac7678_reference_mode reference_mode;
	bool reset_on_init;
	bool configure_clear;
	bool configure_reference;
};

struct dac7x78_data {
	uint8_t configured;
};


static int dac7x78_reg_read(const struct device *dev, uint8_t reg,
			      uint16_t *val)
{
	const struct dac7x78_config *cfg = dev->config;

	if (i2c_burst_read_dt(&cfg->bus, reg, (uint8_t *) val, 2) < 0) {
		LOG_ERR("I2C read failed");
		return -EIO;
	}

	*val = sys_be16_to_cpu(*val);

	return 0;
}

static int dac7x78_reg_write(const struct device *dev, uint8_t control_word,
			       uint16_t val)
{
	const struct dac7x78_config *cfg = dev->config;
	uint8_t buf[3];

	buf[0] = control_word;

	buf[1] = (val >> 4) & 0xFF;
	buf[2] = (val & 0x0F) << 4;

	return i2c_write_dt(&cfg->bus, buf, sizeof(buf));
}


static int __maybe_unused dac7x78_reg_update(const struct device *dev, uint8_t reg,
					     uint16_t mask, bool setting)
{
	uint16_t regval;
	int ret;

	ret = dac7x78_reg_read(dev, reg, &regval);
	if (ret) {
		return -EIO;
	}

	if (setting) {
		regval |= mask;
	} else {
		regval &= ~mask;
	}

	ret = dac7x78_reg_write(dev, reg, regval);
	if (ret) {
		return ret;
	}

	return 0;
}

static int dac7x78_channel_setup(const struct device *dev,
				   const struct dac_channel_cfg *channel_cfg)
{
	const struct dac7x78_config *config = dev->config;
	struct dac7x78_data *data = dev->data;
	uint8_t control_word;
	int ret;

	if (channel_cfg->channel_id > DAC7X78_CHANNEL_COUNT - 1) {
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

	ret = dac7x78_reg_write(dev, control_word, 0);
	if (ret) {
		LOG_ERR("Unable to power up channel %d", channel_cfg->channel_id);
		return -EIO;
	}

	data->configured |= BIT(channel_cfg->channel_id);

	LOG_DBG("Channel %d initialized", channel_cfg->channel_id);

	return 0;
}


static int dac7x78_write_value(const struct device *dev, uint8_t channel,
				uint32_t value)
{
	const struct dac7x78_config *config = dev->config;
	struct dac7x78_data *data = dev->data;
	uint8_t control_word;
	uint16_t regval;
	int ret;

	if (channel > DAC7X78_CHANNEL_COUNT - 1) {
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

	control_word = (channel & 0x0F); /* Input register for channel. */

	regval = (value & 0x0FFF);

	ret = dac7x78_reg_write(dev, control_word, regval);
	if (ret) {
		LOG_ERR("Unable to set value %d on channel %d", value, channel);
		return -EIO;
	}

	return 0;
}

int dac7x78_read_value(const struct device *dev, uint8_t channel,
		       uint32_t *value)
{
	struct dac7x78_data *data = dev->data;
	uint8_t control_word;
	uint16_t regval;
	int ret;

	if (channel > DAC7X78_CHANNEL_COUNT - 1) {
		LOG_ERR("Unsupported channel %d", channel);
		return -ENOTSUP;
	}

	if (!(data->configured & BIT(channel))) {
		LOG_ERR("Channel %d not initialized", channel);
		return -EINVAL;
	}

	control_word = (channel & 0x0F) + 0x10; /* DAC register, not input register. */
	ret = dac7x78_reg_read(dev, control_word, &regval);
	if (ret) {
		LOG_ERR("I2C read value failed");
		return -EIO;
	}

	regval = sys_be16_to_cpu(regval);
	*value = (regval >> 4) & 0x0FFF;

	return 0;
}

static int dac7x78_soft_reset(const struct device *dev)
{
	uint8_t control_word;
	int ret;

	control_word = (0x07 << 4); /* Software reset command. */

	ret = dac7x78_reg_write(dev, control_word, 0);
	if (ret) {
		LOG_ERR("Software reset failed");
		return -EIO;
	}

	k_msleep(DAC7X78_RESET_DELAY_MS);

	return 0;
}

static int dac7x78_configure_clear(const struct device *dev)
{
	const struct dac7x78_config *config = dev->config;
	uint8_t clear_code;
	int ret;

	if (!config->configure_clear || config->clear_mode == DAC7X78_CLEAR_DEFAULT) {
		return 0;
	}

	switch (config->clear_mode) {
	case DAC7X78_CLEAR_ZERO_SCALE:
		clear_code = 0U;
		break;
	case DAC7X78_CLEAR_MIDSCALE:
		clear_code = 1U;
		break;
	case DAC7X78_CLEAR_FULL_SCALE:
		clear_code = 2U;
		break;
	case DAC7X78_CLEAR_DISABLED:
		clear_code = 3U;
		break;
	default:
		return -EINVAL;
	}

	ret = dac7x78_reg_write(dev, 0x50, clear_code);
	if (ret != 0) {
		LOG_ERR("Clear-code configuration failed");
		return -EIO;
	}

	return 0;
}

static int dac7x78_configure_reference(const struct device *dev)
{
	const struct dac7x78_config *config = dev->config;
	int ret;

	if (!config->configure_reference) {
		return 0;
	}

	if (config->reference_mode != DAC7678_REFERENCE_EXTERNAL &&
	    config->model != DAC7X78_MODEL_DAC7678) {
		LOG_ERR("Internal reference requested on device without internal reference");
		return -ENOTSUP;
	}

	switch (config->reference_mode) {
	case DAC7678_REFERENCE_EXTERNAL:
		if (config->model != DAC7X78_MODEL_DAC7678) {
			return 0;
		}
		ret = dac7x78_reg_write(dev, 0x80, 0U);
		break;
	case DAC7678_REFERENCE_INTERNAL_STATIC:
		ret = dac7x78_reg_write(dev, 0x80, 1U);
		break;
	case DAC7678_REFERENCE_INTERNAL_FLEXIBLE:
		/* DAC7678 flexible mode code 0b101 keeps the internal
		 * reference powered regardless of DAC power-down state.
		 */
		ret = dac7x78_reg_write(dev, 0x90, 5U);
		break;
	default:
		return -EINVAL;
	}

	if (ret != 0) {
		LOG_ERR("Reference configuration failed");
		return -EIO;
	}

	return 0;
}

static int dac7x78_init(const struct device *dev)
{
	const struct dac7x78_config *config = dev->config;
	struct dac7x78_data *data = dev->data;
	int ret;

	if (!device_is_ready(config->bus.bus)) {
		LOG_ERR("I2C device not ready");
		return -ENODEV;
	}

	if (config->reset_on_init) {
		ret = dac7x78_soft_reset(dev);
		if (ret) {
			LOG_ERR("Soft-reset failed");
			return ret;
		}
	}

	ret = dac7x78_configure_reference(dev);
	if (ret != 0) {
		return ret;
	}

	ret = dac7x78_configure_clear(dev);
	if (ret != 0) {
		return ret;
	}

	data->configured = 0;

	LOG_DBG("Init complete");

	return 0;
}


static DEVICE_API(dac, dac7x78_driver_api) = {
	.channel_setup =  dac7x78_channel_setup,
	.write_value =  dac7x78_write_value
};

#define DAC7X78_DEVICE(node_id, device_model) \
	static struct dac7x78_data dac7x78_data_##node_id; \
	static const struct dac7x78_config dac7x78_config_##node_id = { \
		.bus = I2C_DT_SPEC_GET(node_id), \
		.resolution = 12, \
		.model = device_model, \
		.clear_mode = DT_ENUM_IDX_OR(node_id, ti_clear_mode, DAC7X78_CLEAR_DEFAULT), \
		.reference_mode = DT_ENUM_IDX_OR(node_id, ti_reference, DAC7678_REFERENCE_EXTERNAL), \
		.reset_on_init = DT_PROP_OR(node_id, ti_reset_on_init, false), \
		.configure_clear = DT_NODE_HAS_PROP(node_id, ti_clear_mode), \
		.configure_reference = DT_NODE_HAS_PROP(node_id, ti_reference), \
	}; \
	DEVICE_DT_DEFINE(node_id, \
				&dac7x78_init, NULL, \
				&dac7x78_data_##node_id, \
				&dac7x78_config_##node_id, POST_KERNEL, \
				CONFIG_DAC7X78_INIT_PRIORITY, \
				&dac7x78_driver_api)

#define DAC7X78_DEVICE_DAC7578(node_id) \
	DAC7X78_DEVICE(node_id, DAC7X78_MODEL_DAC7578);

#define DAC7X78_DEVICE_DAC7678(node_id) \
	DAC7X78_DEVICE(node_id, DAC7X78_MODEL_DAC7678);

DT_FOREACH_STATUS_OKAY(ti_dac7578, DAC7X78_DEVICE_DAC7578)
DT_FOREACH_STATUS_OKAY(ti_dac7678, DAC7X78_DEVICE_DAC7678)
