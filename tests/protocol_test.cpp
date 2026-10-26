#include "components/bthome/protocol.h"

#include <cassert>
#include <iostream>
#include <limits>

int main() {
  using namespace microbit_bthome;
  const Advertisement expected = {
      0x02, 0x01, 0x06, 0x0C, 0x16, 0xD2, 0xFC, 0x40,
      0x02, 0xC4, 0x09, 0x0F, 0x01, 0x51, 0x4F, 0x26,
  };
  assert(encode({2500, true, 9807}) == expected);
  for (int temperature : {-32768, -2500, -1, 0, 2500, 32767}) {
    for (bool pressed : {false, true}) {
      for (uint16_t acceleration : {uint16_t{0}, uint16_t{9807}, uint16_t{65535}}) {
        const auto packet = encode({static_cast<int16_t>(temperature), pressed, acceleration});
        assert(packet[0] + 1 == 3);
        assert(packet[3] + 4 == packet.size());
        Reading decoded{};
        assert(decode(packet.data() + 7, packet.size() - 7, decoded) == DecodeResult::OK);
        assert(decoded.temperature_centi == temperature);
        assert(decoded.button_pressed == pressed);
        assert(decoded.acceleration_milli == acceleration);
      }
    }
  }
  Reading unchanged{123, true, 456};
  for (size_t size = 0; size < 9; ++size)
    assert(decode(expected.data() + 7, size, unchanged) == DecodeResult::BAD_LENGTH);
  assert(decode(expected.data() + 7, 10, unchanged) == DecodeResult::BAD_LENGTH);
  for (const auto index : {7, 8, 11, 12, 13}) {
    auto invalid = expected;
    invalid[index] = 0xFF;
    assert(decode(invalid.data() + 7, 9, unchanged) != DecodeResult::OK);
    assert(unchanged.temperature_centi == 123);
    assert(unchanged.button_pressed);
    assert(unchanged.acceleration_milli == 456);
  }
  auto encrypted = expected;
  encrypted[7] = 0x41;
  assert(decode(encrypted.data() + 7, 9, unchanged) == DecodeResult::BAD_INFO);
  std::cout << "BTHome byte layout, signed values, bounds and malformed packets: PASS\n";
}