#include "bthome.h"
#include "protocol.h"
#include "esphome/core/log.h"

#include <cmath>

namespace esphome {
namespace bthome {

static const char *const TAG = "bthome";

bool BTHomeHub::parse_device(const ble_device_base::ESPBTDevice &device) {
  if (device.address_uint64() != address_)
    return false;

  for (const auto &service : device.get_service_datas()) {
    if (service.uuid != ble_device_base::ESPBTUUID::from_uint16(0xFCD2))
      continue;

    microbit_bthome::Reading reading{};
    const auto result = microbit_bthome::decode(service.data.data(), service.data.size(), reading);
    if (result != microbit_bthome::DecodeResult::OK) {
      const char *reason = "unexpected object layout (expected 02,0F,51)";
      if (result == microbit_bthome::DecodeResult::BAD_LENGTH)
        reason = "invalid service-data length (expected 9 bytes after UUID)";
      else if (result == microbit_bthome::DecodeResult::BAD_INFO)
        reason = "unsupported device info (expected unencrypted V2 0x40)";
      else if (result == microbit_bthome::DecodeResult::BAD_BOOLEAN)
        reason = "invalid boolean (expected 0 or 1)";
      ESP_LOGW(TAG, "Dropped BTHome packet: %s", reason);
      return false;
    }

    if (temperature_ != nullptr)
      temperature_->publish_state(reading.temperature_centi * 0.01f);
    if (acceleration_ != nullptr)
      acceleration_->publish_state(reading.acceleration_milli * 0.001f);
    if (button_ != nullptr)
      button_->publish_state(reading.button_pressed);
    ESP_LOGD(TAG, "Accepted V2 packet: temperature=%.2f button=%u acceleration=%.3f RSSI=%d",
             reading.temperature_centi * 0.01f, static_cast<unsigned>(reading.button_pressed),
             reading.acceleration_milli * 0.001f, device.get_rssi());
    this->set_timeout("stale", stale_timeout_, [this]() {
      ESP_LOGW(TAG, "No valid BTHome packet within stale_timeout; readings unknown");
      if (temperature_ != nullptr)
        temperature_->publish_state(NAN);
      if (acceleration_ != nullptr)
        acceleration_->publish_state(NAN);
      if (button_ != nullptr)
        button_->invalidate_state();
    });
    return true;
  }
  ESP_LOGV(TAG, "Ignored configured MAC: no BTHome 0xFCD2 service data");
  return false;
}

void BTHomeHub::dump_config() {
  ESP_LOGCONFIG(TAG, "BTHome V2 micro:bit profile (unencrypted; objects 02,0F,51)");
  LOG_SENSOR("  ", "Temperature", temperature_);
  LOG_SENSOR("  ", "Total acceleration", acceleration_);
  LOG_BINARY_SENSOR("  ", "Button A", button_);
}

}
}