#pragma once

#include <cmath>

namespace tsndgm {

typedef unsigned int FrameSize;
typedef unsigned int StreamID;
typedef unsigned long BurstSize;
typedef unsigned int DeviceId;
typedef unsigned int LinkId;
// Tick: time in nanoseconds
typedef long Tick;
typedef Tick Delay;
// DataRate: Byte per second
typedef unsigned long DataRate;

typedef std::pair<DeviceId, DeviceId> Edge;

/**
 * @param mbps data rate in mega bit.
 * Attention: can only process positive inputs safely.
 * @return data rate in byte per second
 */
constexpr auto mbps_to_DataRate(const float mbps) -> DataRate {
  // 10^6: mega, 8: bit to byte
  constexpr auto factor = 1e6 / 8;
  return static_cast<DataRate>(std::round(mbps * factor));
}

constexpr auto seconds_to_ticks(const double seconds) -> Tick {
  // 10^9: nano, 1: seconds to nano seconds
  return static_cast<Tick>(std::ceil(seconds * 1e9));
}

[[nodiscard]] constexpr auto calculateTransmissionDelay(const DataRate data_rate, const FrameSize frame_size) -> Delay {
  const long double factor = frame_size * 1.0e9L;
  const auto dtrans = static_cast<Delay>(std::ceil(factor / data_rate));

  return dtrans;
}

} // namespace tsndgm
