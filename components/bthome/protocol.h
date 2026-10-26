#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace microbit_bthome {

using Advertisement = std::array<uint8_t, 21>;

struct Reading {
  int16_t temperature_centi;
  bool button_pressed;
  uint16_t acceleration_milli;
  bool button_b_pressed{false};
  uint16_t heading_centi{0};
  bool has_extended_data{true};
};

inline Advertisement encode(const Reading &reading) {
  const auto temperature = static_cast<uint16_t>(reading.temperature_centi);
  return {
      0x02, 0x01, 0x06,
      0x11, 0x16, 0xD2, 0xFC, 0x40,
      0x02, static_cast<uint8_t>(temperature), static_cast<uint8_t>(temperature >> 8),
      0x0F, static_cast<uint8_t>(reading.button_pressed),
      0x0F, static_cast<uint8_t>(reading.button_b_pressed),
      0x51, static_cast<uint8_t>(reading.acceleration_milli),
      static_cast<uint8_t>(reading.acceleration_milli >> 8),
      0x5E, static_cast<uint8_t>(reading.heading_centi),
      static_cast<uint8_t>(reading.heading_centi >> 8),
  };
}

enum class DecodeResult { OK, BAD_LENGTH, BAD_INFO, BAD_OBJECTS, BAD_BOOLEAN, BAD_HEADING };

inline DecodeResult decode(const uint8_t *data, size_t size, Reading &reading) {
  if (size != 9 && size != 14)
    return DecodeResult::BAD_LENGTH;
  if (data[0] != 0x40)
    return DecodeResult::BAD_INFO;
  const bool extended = size == 14;
  const size_t acceleration_index = extended ? 8 : 6;
  if (data[1] != 0x02 || data[4] != 0x0F || data[acceleration_index] != 0x51 ||
      (extended && (data[6] != 0x0F || data[11] != 0x5E)))
    return DecodeResult::BAD_OBJECTS;
  if (data[5] > 1 || (extended && data[7] > 1))
    return DecodeResult::BAD_BOOLEAN;

  const uint16_t heading = extended
                               ? static_cast<uint16_t>(data[12] | (static_cast<uint16_t>(data[13]) << 8))
                               : 0;
  if (heading >= 36000)
    return DecodeResult::BAD_HEADING;

  const uint16_t raw_temperature = data[2] | (static_cast<uint16_t>(data[3]) << 8);
  const int32_t temperature = raw_temperature >= 0x8000
                                  ? static_cast<int32_t>(raw_temperature) - 0x10000
                                  : raw_temperature;
  reading = {static_cast<int16_t>(temperature), data[5] == 1,
             static_cast<uint16_t>(data[acceleration_index + 1] |
                                   (static_cast<uint16_t>(data[acceleration_index + 2]) << 8)),
             extended && data[7] == 1, heading, extended};
  return DecodeResult::OK;
}

}