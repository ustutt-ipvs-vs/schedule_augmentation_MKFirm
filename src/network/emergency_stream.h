#pragma once

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
  DataRate refill_rate;

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
  auto stream = EmergencyStream{.id = j["streamID"],
                                .name = j["name"],
                                .source = j["source"],
                                .destination = j["target"],
                                .bucket_size_byte = j["bucket_size_byte"],
                                .refill_rate = mbps_to_DataRate(j["rate_mbps"])};
  PathRoute route;
  route.reserve(j["route"].size());
  for (const auto &hop : j["route"]) {
    route.emplace_back(hop["from"], hop["to"]);
  }
  stream.route = RouteWrapper{std::move(route)};
  return stream;
}

} // namespace tsndgm
