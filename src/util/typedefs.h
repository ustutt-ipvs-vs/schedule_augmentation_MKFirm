#pragma once

#include <cmath>

namespace tsndgm {
typedef unsigned int FrameSize;
typedef unsigned int StreamID;
typedef unsigned long BurstSize;
typedef unsigned int DeviceId;
typedef unsigned int LinkId;
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
} // namespace tsndgm
