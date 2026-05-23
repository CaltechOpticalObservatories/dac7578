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

#include "drivers/dac/dac7578.h"

#define LOG_LEVEL CONFIG_GPIO_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(i2c_dac7578);

#define DT_DRV_COMPAT ti_dac7578

enum dac7578_clear_mode {
	DAC7578_CLEAR_DEFAULT,
	DAC7578_CLEAR_ZERO_SCALE,
	DAC7578_CLEAR_MIDSCALE,
	DAC7578_CLEAR_FULL_SCALE,
	DAC7578_CLEAR_DISABLED,
};

enum dac7578_reference_mode {
	DAC7578_REF_EXTERNAL,
	DAC7578_REF_INTERNAL_STATIC,
	DAC7578_REF_INTERNAL_FLEXIBLE,
};

enum dac7578_device_type {
	DAC7578_TYPE_DAC7578,
	DAC7578_TYPE_DAC7678,
};

struct dac7578_config {
	struct i2c_dt_spec bus;
	uint8_t resolution;
	enum dac7578_device_type type;
	enum dac7578_clear_mode clear_mode;
	enum dac7578_reference_mode reference_mode;
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

	control_word = (channel & 0x0F); /* Input register for channel. */

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

	control_word = (channel & 0x0F) + 0x10; /* DAC register, not input register. */
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

	control_word = (0x07 << 4); /* Software reset command. */

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

static int dac7578_configure_clear(const struct device *dev)
{
	const struct dac7578_config *config = dev->config;
	uint8_t clear_code;
	int ret;

	if (config->clear_mode == DAC7578_CLEAR_DEFAULT) {
		return 0;
	}

	switch (config->clear_mode) {
	case DAC7578_CLEAR_ZERO_SCALE:
		clear_code = 0U;
		break;
	case DAC7578_CLEAR_MIDSCALE:
		clear_code = 1U;
		break;
	case DAC7578_CLEAR_FULL_SCALE:
		clear_code = 2U;
		break;
	case DAC7578_CLEAR_DISABLED:
		clear_code = 3U;
		break;
	default:
		return -EINVAL;
	}

	ret = dac7578_reg_write(dev, 0x50, clear_code);
	if (ret != 0) {
		LOG_ERR("Clear-code configuration failed");
		return -EIO;
	}

	return 0;
}

static int dac7578_configure_reference(const struct device *dev)
{
	const struct dac7578_config *config = dev->config;
	int ret;

	if (config->reference_mode != DAC7578_REF_EXTERNAL &&
	    config->type != DAC7578_TYPE_DAC7678) {
		LOG_ERR("Internal reference requested on device without internal reference");
		return -ENOTSUP;
	}

	switch (config->reference_mode) {
	case DAC7578_REF_EXTERNAL:
		if (config->type != DAC7578_TYPE_DAC7678) {
			return 0;
		}
		ret = dac7578_reg_write(dev, 0x80, 0U);
		break;
	case DAC7578_REF_INTERNAL_STATIC:
		ret = dac7578_reg_write(dev, 0x80, 1U);
		break;
	case DAC7578_REF_INTERNAL_FLEXIBLE:
		/* DAC7678 flexible mode code 0b101 keeps the internal
		 * reference powered regardless of DAC power-down state.
		 */
		ret = dac7578_reg_write(dev, 0x90, 5U);
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

	ret = dac7578_configure_reference(dev);
	if (ret != 0) {
		return ret;
	}

	ret = dac7578_configure_clear(dev);
	if (ret != 0) {
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

#define DAC7578_DEVICE(node_id, dev_type) \
	static struct dac7578_data dac7578_data_##node_id; \
	static const struct dac7578_config dac7578_config_##node_id = { \
		.bus = I2C_DT_SPEC_GET(node_id), \
		.resolution = 12, \
		.type = dev_type, \
		.clear_mode = DT_ENUM_IDX_OR(node_id, ti_clear_mode, DAC7578_CLEAR_DEFAULT), \
		.reference_mode = DT_ENUM_IDX_OR(node_id, ti_reference, DAC7578_REF_EXTERNAL), \
	}; \
	DEVICE_DT_DEFINE(node_id, \
				&dac7578_init, NULL, \
				&dac7578_data_##node_id, \
				&dac7578_config_##node_id, POST_KERNEL, \
				CONFIG_DAC7578_INIT_PRIORITY, \
				&dac7578_driver_api)

#define DAC7578_DEVICE_DAC7578(node_id) \
	DAC7578_DEVICE(node_id, DAC7578_TYPE_DAC7578);

#define DAC7578_DEVICE_DAC7678(node_id) \
	DAC7578_DEVICE(node_id, DAC7578_TYPE_DAC7678);

DT_FOREACH_STATUS_OKAY(ti_dac7578, DAC7578_DEVICE_DAC7578)
DT_FOREACH_STATUS_OKAY(ti_dac7678, DAC7578_DEVICE_DAC7678)
