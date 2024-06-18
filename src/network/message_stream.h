#pragma once

#include "../util/typedefs.h"
#include "route.h"
#include "topology.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace tsndgm {

typedef std::map<Edge, Delay> DelayMap;
typedef std::list<Edge> WirelessLinks;

typedef unsigned int MessageStreamHandle;

struct MessageStream {
  StreamID id;
  std::string name;

  Tick period;
  FrameSize frame_size;
  Delay deadline;
  Delay jitter = 0; // TODO discuss if jitter and phase are needed and if they should be here.
  Delay phase = 0;
  RouteWrapper route;

  [[nodiscard]] auto dump() const -> nlohmann::json;
};

auto createMessageStream(const nlohmann::json &j) -> MessageStream;

void dump_streams(const std::vector<MessageStream> &streams, const std::filesystem::path &out);

} // namespace tsndgm
