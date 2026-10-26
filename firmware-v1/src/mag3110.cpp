#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/byteorder.h>

#include <cerrno>
#include <cstdint>

namespace {

struct Mag3110Config {
  i2c_dt_spec bus;
};

struct Mag3110Data {
  sensor_value axes[3];
};

int mag3110_init(const device *sensor) {
  const auto *config = static_cast<const Mag3110Config *>(sensor->config);
  if (!i2c_is_ready_dt(&config->bus))
    return -ENODEV;
  uint8_t identity = 0;
  int error = i2c_reg_read_byte_dt(&config->bus, 0x07, &identity);
  if (error != 0)
    return error;
  if (identity != 0xC4)
    return -ENODEV;
  error = i2c_reg_write_byte_dt(&config->bus, 0x10, 0x00);
  if (error != 0)
    return error;
  error = i2c_reg_write_byte_dt(&config->bus, 0x11, 0x80);
  if (error != 0)
    return error;
  return i2c_reg_write_byte_dt(&config->bus, 0x10, 0xC1);
}

int mag3110_fetch(const device *sensor, sensor_channel channel) {
  if (channel != SENSOR_CHAN_ALL && channel != SENSOR_CHAN_MAGN_XYZ)
    return -ENOTSUP;
  const auto *config = static_cast<const Mag3110Config *>(sensor->config);
  auto *data = static_cast<Mag3110Data *>(sensor->data);
  uint8_t status = 0;
  int error = i2c_reg_read_byte_dt(&config->bus, 0x00, &status);
  if (error != 0)
    return error;
  if ((status & 0x08) == 0)
    return -EAGAIN;
  uint8_t raw[6];
  error = i2c_burst_read_dt(&config->bus, 0x01, raw, sizeof(raw));
  if (error != 0)
    return error;
  for (size_t axis = 0; axis < 3; ++axis) {
    const uint16_t encoded = sys_get_be16(raw + axis * 2);
    const int32_t value = encoded >= 0x8000 ? static_cast<int32_t>(encoded) - 0x10000 : encoded;
    data->axes[axis].val1 = value / 1000;
    data->axes[axis].val2 = (value % 1000) * 1000;
  }
  return 0;
}

int mag3110_get(const device *sensor, sensor_channel channel, sensor_value *values) {
  if (channel != SENSOR_CHAN_MAGN_XYZ)
    return -ENOTSUP;
  const auto *data = static_cast<const Mag3110Data *>(sensor->data);
  for (size_t axis = 0; axis < 3; ++axis)
    values[axis] = data->axes[axis];
  return 0;
}

static DEVICE_API(sensor, mag3110_api) = {
    .sample_fetch = mag3110_fetch,
    .channel_get = mag3110_get,
};

static const Mag3110Config mag3110_config = {
    I2C_DT_SPEC_GET(DT_NODELABEL(mag3110)),
};
static Mag3110Data mag3110_data{};

}

DEVICE_DT_DEFINE(DT_NODELABEL(mag3110), mag3110_init, nullptr, &mag3110_data,
                 &mag3110_config, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY, &mag3110_api);