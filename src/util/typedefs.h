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
// DataRate: Byte per nanosecond
typedef double DataRate;

typedef std::pair<DeviceId, DeviceId> Edge;

/**
 * @param mbps data rate in mega bit.
 * Attention: can only process positive inputs safely.
 * @return data rate in byte per second
 */
constexpr auto mbps_to_DataRate(const double mbps) -> DataRate {
  // 10^6: mega, 8: bit to byte
  constexpr auto factor = 1e6 / 8;
  return static_cast<DataRate>((mbps * factor / 1e9));
}

[[nodiscard]] constexpr auto calculateTransmissionDelay(const DataRate data_rate, const FrameSize frame_size) -> Delay {
  const auto dtrans = frame_size / data_rate;
  return static_cast<Delay>(std::ceil(dtrans));
}

} // namespace tsndgm
