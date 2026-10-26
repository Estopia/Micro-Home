#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace microbit_bthome {

using Advertisement = std::array<uint8_t, 16>;

struct Reading {
  int16_t temperature_centi;
  bool button_pressed;
  uint16_t acceleration_milli;
};

inline Advertisement encode(const Reading &reading) {
  const auto temperature = static_cast<uint16_t>(reading.temperature_centi);
  return {
      0x02, 0x01, 0x06,
      0x0C, 0x16, 0xD2, 0xFC, 0x40,
      0x02, static_cast<uint8_t>(temperature), static_cast<uint8_t>(temperature >> 8),
      0x0F, static_cast<uint8_t>(reading.button_pressed),
      0x51, static_cast<uint8_t>(reading.acceleration_milli),
      static_cast<uint8_t>(reading.acceleration_milli >> 8),
  };
}

enum class DecodeResult { OK, BAD_LENGTH, BAD_INFO, BAD_OBJECTS, BAD_BOOLEAN };

inline DecodeResult decode(const uint8_t *data, size_t size, Reading &reading) {
  if (size != 9)
    return DecodeResult::BAD_LENGTH;
  if (data[0] != 0x40)
    return DecodeResult::BAD_INFO;
  if (data[1] != 0x02 || data[4] != 0x0F || data[6] != 0x51)
    return DecodeResult::BAD_OBJECTS;
  if (data[5] > 1)
    return DecodeResult::BAD_BOOLEAN;

  const uint16_t raw_temperature = data[2] | (static_cast<uint16_t>(data[3]) << 8);
  const int32_t temperature = raw_temperature >= 0x8000
                                  ? static_cast<int32_t>(raw_temperature) - 0x10000
                                  : raw_temperature;
  reading = {static_cast<int16_t>(temperature), data[5] == 1,
             static_cast<uint16_t>(data[7] | (static_cast<uint16_t>(data[8]) << 8))};
  return DecodeResult::OK;
}

}