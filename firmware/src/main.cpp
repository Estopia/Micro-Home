// Toolchain: Zephyr v4.2.0 SDK; firmware/ targets v2, firmware-v1/ targets early v1.
#include "protocol.h"
#include "compass.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/reboot.h>
#include <hal/nrf_ficr.h>

#include <cmath>
#include <cerrno>
#include <cstdint>

LOG_MODULE_REGISTER(microbit_bthome, LOG_LEVEL_INF);

static const device *const temperature_sensor = DEVICE_DT_GET(DT_NODELABEL(temp));
static const device *const accelerometer = DEVICE_DT_GET(DT_ALIAS(accel0));
static const device *const magnetometer = DEVICE_DT_GET(DT_ALIAS(magn0));
static const gpio_dt_spec button_a = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const gpio_dt_spec button_b = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);

static constexpr int64_t SAMPLE_PERIOD_MS = 5000;
static constexpr int64_t BURST_MS = 600;
static constexpr double MAGNETIC_X_OFFSET_GAUSS = 0.0;
static constexpr double MAGNETIC_Y_OFFSET_GAUSS = 0.0;

static int initialise_identity() {
  bt_addr_le_t identity{};
  identity.type = BT_ADDR_LE_RANDOM;
  sys_put_le32(NRF_FICR->DEVICEADDR[0], identity.a.val);
  sys_put_le16(static_cast<uint16_t>(NRF_FICR->DEVICEADDR[1]), identity.a.val + 4);
  identity.a.val[5] |= 0xC0;
  const int identity_id = bt_id_create(&identity, nullptr);
  if (identity_id < 0)
    return identity_id;
  return identity_id == BT_ID_DEFAULT ? 0 : -EINVAL;
}

static int sample(microbit_bthome::Reading &reading) {
  sensor_value temperature{};
  sensor_value axes[3]{};
  sensor_value magnetic_axes[3]{};
  int error = sensor_sample_fetch(temperature_sensor);
  if (error != 0)
    return error;
  error = sensor_channel_get(temperature_sensor, SENSOR_CHAN_DIE_TEMP, &temperature);
  if (error != 0)
    return error;
  error = sensor_sample_fetch(accelerometer);
  if (error != 0)
    return error;
  error = sensor_channel_get(accelerometer, SENSOR_CHAN_ACCEL_XYZ, axes);
  if (error != 0)
    return error;
  error = sensor_sample_fetch(magnetometer);
  if (error != 0)
    return error;
  error = sensor_channel_get(magnetometer, SENSOR_CHAN_MAGN_XYZ, magnetic_axes);
  if (error != 0)
    return error;

  const int first = gpio_pin_get_dt(&button_a);
  if (first < 0)
    return first;
  const int first_b = gpio_pin_get_dt(&button_b);
  if (first_b < 0)
    return first_b;
  k_msleep(25);
  const int second = gpio_pin_get_dt(&button_a);
  if (second < 0)
    return second;
  const int second_b = gpio_pin_get_dt(&button_b);
  if (second_b < 0)
    return second_b;
  if (first != second || first_b != second_b)
    return -EAGAIN;

  const long temperature_centi = std::lround(sensor_value_to_double(&temperature) * 100.0);
  double sum_of_squares = 0.0;
  for (const auto &axis : axes) {
    const double acceleration = sensor_value_to_double(&axis);
    sum_of_squares += acceleration * acceleration;
  }
  const long acceleration_milli = std::lround(std::sqrt(sum_of_squares) * 1000.0);
  if (temperature_centi < INT16_MIN || temperature_centi > INT16_MAX ||
      acceleration_milli < 0 || acceleration_milli > UINT16_MAX)
    return -ERANGE;

  uint16_t heading_centi = 0;
  if (!microbit_bthome::magnetic_heading(
          sensor_value_to_double(&magnetic_axes[0]) - MAGNETIC_X_OFFSET_GAUSS,
          sensor_value_to_double(&magnetic_axes[1]) - MAGNETIC_Y_OFFSET_GAUSS, heading_centi))
    return -ERANGE;

  reading = {static_cast<int16_t>(temperature_centi), second != 0,
             static_cast<uint16_t>(acceleration_milli), second_b != 0, heading_centi};
  return 0;
}

static int advertise(const microbit_bthome::Reading &reading) {
  const auto packet = microbit_bthome::encode(reading);
  const bt_data elements[] = {
      {packet[1], static_cast<uint8_t>(packet[0] - 1), packet.data() + 2},
      {packet[4], static_cast<uint8_t>(packet[3] - 1), packet.data() + 5},
  };
  const bt_le_adv_param parameters = BT_LE_ADV_PARAM_INIT(
      BT_LE_ADV_OPT_USE_IDENTITY, 0x00A0, 0x00B0, nullptr);
  const int error = bt_le_adv_start(&parameters, elements, ARRAY_SIZE(elements), nullptr, 0);
  if (error != 0)
    return error;
  k_msleep(BURST_MS);
  return bt_le_adv_stop();
}

int main() {
    if (!device_is_ready(temperature_sensor) || !device_is_ready(accelerometer) ||
      !device_is_ready(magnetometer) || !gpio_is_ready_dt(&button_a) ||
      !gpio_is_ready_dt(&button_b)) {
    LOG_ERR("Sensor/GPIO not ready; check board revision and accelerometer model");
    return -ENODEV;
  }
  int error = gpio_pin_configure_dt(&button_a, GPIO_INPUT | GPIO_PULL_UP);
  if (error == 0)
    error = gpio_pin_configure_dt(&button_b, GPIO_INPUT | GPIO_PULL_UP);
  if (error != 0) {
    LOG_ERR("Button configuration failed: %d", error);
    return error;
  }
  error = initialise_identity();
  if (error == 0)
    error = bt_enable(nullptr);
  if (error != 0) {
    LOG_ERR("Bluetooth initialisation failed: %d", error);
    return error;
  }

  bt_addr_le_t identity{};
  size_t count = 1;
  bt_id_get(&identity, &count);
  char address[BT_ADDR_LE_STR_LEN];
  if (count != 1)
    return -ENODEV;
  bt_addr_le_to_str(&identity, address, sizeof(address));
  LOG_INF("BTHome MAC: %s", address);
  k_msleep(1100);

  int64_t next_sample = k_uptime_get();
  while (true) {
    next_sample += SAMPLE_PERIOD_MS;
    microbit_bthome::Reading reading{};
    error = sample(reading);
    if (error != 0) {
      LOG_WRN("Sample skipped: %d", error);
    } else {
      error = advertise(reading);
      if (error != 0) {
        LOG_ERR("Advertising failed: %d; rebooting", error);
        sys_reboot(SYS_REBOOT_COLD);
      }
    }
    const int64_t remaining = next_sample - k_uptime_get();
    if (remaining > 0)
      k_msleep(static_cast<int32_t>(remaining));
    else
      next_sample = k_uptime_get();
  }
}