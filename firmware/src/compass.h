#pragma once

#include <cmath>
#include <cstdint>

namespace microbit_bthome {

inline bool magnetic_heading(double magnetic_x, double magnetic_y, uint16_t &heading_centi) {
  if (!std::isfinite(magnetic_x) || !std::isfinite(magnetic_y) ||
      std::hypot(magnetic_x, magnetic_y) < 0.000001)
    return false;
  constexpr double RADIANS_TO_DEGREES = 180.0 / 3.14159265358979323846;
  double degrees = std::atan2(-magnetic_y, magnetic_x) * RADIANS_TO_DEGREES;
  if (degrees < 0.0)
    degrees += 360.0;
  heading_centi = static_cast<uint16_t>(std::lround(degrees * 100.0) % 36000);
  return true;
}

}