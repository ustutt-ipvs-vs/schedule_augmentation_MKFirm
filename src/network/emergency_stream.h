#pragma once

#include "../util/constants.h"
#include "../util/typedefs.h"
#include "route.h"
#include <nlohmann/json.hpp>
#include <string>

namespace tsndgm {

struct EmergencyStream {
  StreamID id;
  std::string name;
  DeviceId source;
  DeviceId destination;
  BurstSize bucket_size_byte;
  BurstSize bucket_size_byte_without_interframe_gap;
  DataRate refill_rate;
  DataRate refill_rate_without_interframe_gap;

  RouteWrapper route;

  [[nodiscard]] auto dump() const -> nlohmann::json {
    nlohmann::json j = {{"id", id},
                        {"name", name},
                        {"source", source},
                        {"destination", destination},
                        {"burst size", bucket_size_byte},
                        {"refill rate", refill_rate},
                        {"route", {}}};

    for (const auto &hop : route.route) {
      j["route"].push_back(hop);
    }
    return j;
  }

  [[nodiscard]] auto to_string() const -> std::string {
    std::stringstream ss;
    ss << id << ": " << name << "\t";
    ss << source << "->" << destination << "\t";
    ss << "b: " << bucket_size_byte << " r: " << refill_rate;

    return ss.str();
  }
};

inline auto createEmergencyStream(const nlohmann::json &j) -> EmergencyStream {
  // To incorporate the interframe gaps between emergency packets, we assume that every 64B packet actually requires a
  // size of 76B (= 64B + 12B interframe gap).
  auto stream = EmergencyStream{
      .id = j["streamID"],
      .name = j["name"],
      .source = j["source"],
      .destination = j["target"],
      .bucket_size_byte =
          static_cast<BurstSize>(ceil(static_cast<double>(j["bucket_size_byte"]) * INTER_FRAME_GAP_INCREASE_FACTOR)),
      .bucket_size_byte_without_interframe_gap = j["bucket_size_byte"],
      .refill_rate = mbps_to_DataRate(static_cast<double>(j["rate_mbps"]) * INTER_FRAME_GAP_INCREASE_FACTOR),
      .refill_rate_without_interframe_gap = mbps_to_DataRate(j["rate_mbps"])};
  PathRoute route;
  route.reserve(j["route"].size());
  for (const auto &hop : j["route"]) {
    route.emplace_back(hop["from"], hop["to"]);
  }
  stream.route = RouteWrapper{std::move(route)};
  return stream;
}

} // namespace tsndgm
