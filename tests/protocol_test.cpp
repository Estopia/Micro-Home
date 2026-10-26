#include "components/bthome/protocol.h"
#include "firmware/src/compass.h"

#include <cassert>
#include <iostream>
#include <limits>

int main() {
  using namespace microbit_bthome;
  uint16_t heading = 123;
  assert(magnetic_heading(1.0, 0.0, heading) && heading == 0);
  assert(magnetic_heading(0.0, -1.0, heading) && heading == 9000);
  assert(magnetic_heading(-1.0, 0.0, heading) && heading == 18000);
  assert(magnetic_heading(0.0, 1.0, heading) && heading == 27000);
  assert(magnetic_heading(1.0, -1.0, heading) && heading == 4500);
  assert(magnetic_heading(1.0, 0.0000001, heading) && heading == 0);
  assert(!magnetic_heading(0.0, 0.0, heading));
  assert(!magnetic_heading(std::numeric_limits<double>::infinity(), 1.0, heading));
  assert(!magnetic_heading(1.0, std::numeric_limits<double>::quiet_NaN(), heading));
  const Advertisement expected = {
      0x02, 0x01, 0x06, 0x11, 0x16, 0xD2, 0xFC, 0x40,
      0x02, 0xC4, 0x09, 0x0F, 0x01, 0x0F, 0x00, 0x51, 0x4F, 0x26,
      0x5E, 0x28, 0x23,
  };
    assert(encode({2500, true, 9807, false, 9000}) == expected);
  for (int temperature : {-32768, -2500, -1, 0, 2500, 32767}) {
    for (bool pressed : {false, true}) {
      for (uint16_t acceleration : {uint16_t{0}, uint16_t{9807}, uint16_t{65535}}) {
        for (bool button_b : {false, true}) {
          for (uint16_t heading : {uint16_t{0}, uint16_t{9000}, uint16_t{35999}}) {
            const auto packet = encode({static_cast<int16_t>(temperature), pressed, acceleration,
                                        button_b, heading});
            assert(packet[0] + 1 == 3);
            assert(packet[3] + 4 == packet.size());
            Reading decoded{};
            assert(decode(packet.data() + 7, packet.size() - 7, decoded) == DecodeResult::OK);
            assert(decoded.temperature_centi == temperature);
            assert(decoded.button_pressed == pressed);
            assert(decoded.acceleration_milli == acceleration);
            assert(decoded.button_b_pressed == button_b);
            assert(decoded.heading_centi == heading);
            assert(decoded.has_extended_data);
          }
        }
      }
    }
  }
  Reading unchanged{123, true, 456};
  for (size_t size = 0; size <= 15; ++size) {
    if (size != 9 && size != 14)
      assert(decode(expected.data() + 7, size, unchanged) == DecodeResult::BAD_LENGTH);
  }
  for (const auto index : {7, 8, 11, 12, 13, 14, 15, 18}) {
    auto invalid = expected;
    invalid[index] = 0xFF;
    assert(decode(invalid.data() + 7, 14, unchanged) != DecodeResult::OK);
    assert(unchanged.temperature_centi == 123);
    assert(unchanged.button_pressed);
    assert(unchanged.acceleration_milli == 456);
  }
  auto encrypted = expected;
  encrypted[7] = 0x41;
  assert(decode(encrypted.data() + 7, 14, unchanged) == DecodeResult::BAD_INFO);
  auto invalid_heading = expected;
  invalid_heading[19] = 0xA0;
  invalid_heading[20] = 0x8C;
  assert(decode(invalid_heading.data() + 7, 14, unchanged) == DecodeResult::BAD_HEADING);
  const uint8_t legacy[] = {0x40, 0x02, 0xC4, 0x09, 0x0F, 0x01, 0x51, 0x4F, 0x26};
  Reading decoded_legacy{};
  assert(decode(legacy, sizeof(legacy), decoded_legacy) == DecodeResult::OK);
  assert(decoded_legacy.temperature_centi == 2500);
  assert(decoded_legacy.button_pressed);
  assert(decoded_legacy.acceleration_milli == 9807);
  assert(!decoded_legacy.has_extended_data);
  std::cout << "BTHome layout, both buttons, compass, legacy and malformed packets: PASS\n";
}