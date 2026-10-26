#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace bthome {

class BTHomeHub : public Component, public ble_device_base::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; }
  void set_stale_timeout(uint32_t timeout) { stale_timeout_ = timeout; }
  void set_temperature(sensor::Sensor *entity) { temperature_ = entity; }
  void set_acceleration(sensor::Sensor *entity) { acceleration_ = entity; }
  void set_heading(sensor::Sensor *entity) { heading_ = entity; }
  void set_button(binary_sensor::BinarySensor *entity) { button_ = entity; }
  void set_button_b(binary_sensor::BinarySensor *entity) { button_b_ = entity; }
  bool parse_device(const ble_device_base::ESPBTDevice &device) override;
  void dump_config() override;

 protected:
  uint64_t address_{0};
  uint32_t stale_timeout_{30000};
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *acceleration_{nullptr};
  sensor::Sensor *heading_{nullptr};
  binary_sensor::BinarySensor *button_{nullptr};
  binary_sensor::BinarySensor *button_b_{nullptr};
};

}
}