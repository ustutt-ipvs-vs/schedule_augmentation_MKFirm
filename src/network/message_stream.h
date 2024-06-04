#ifndef TSN_DGM_MESSAGE_STREAM_H
#define TSN_DGM_MESSAGE_STREAM_H

#include "route.h"
#include "topology.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace tsndgm {

typedef unsigned int FrameSize;

class RTI {
public:
  RTI(){};
  RTI(Delay max, Delay min = 0, Delay prop = 0, Delay proc = 0,
      Delay wireline = 0)
      : min(min), max(max), prop(prop), proc(proc), wireline(wireline) {}

  Delay d_max() const { return max + prop + proc + wireline; }
  Delay d_min() const { return min + proc + wireline; }
  Delay d_trans_max() const { return max + prop; }
  Delay d_trans_min() const { return min; }
  Delay d_wireline() const { return wireline; }

  void add_wireline(Delay d) { wireline = d; }

private:
  Delay max;
  Delay min;
  Delay prop;
  Delay proc;
  Delay wireline;
};

static RTI WIRED_RTI(FrameSize frame_size, DataRate data_rate, Delay prop = 0,
                     Delay proc = 0) {
  return RTI(frame_size * SECONDS_TO_TICKS(1) / data_rate,
             frame_size * SECONDS_TO_TICKS(1) / data_rate, prop, proc);
}

typedef std::map<Edge, RTI> RTIMap;
typedef std::map<Edge, Delay> DelayMap;
typedef std::list<Edge> WirelessLinks;

typedef unsigned int MessageStreamHandle;
typedef double Weight;

class MessageStream {
public:
  std::shared_ptr<Route> route;

  Tick period;
  FrameSize frame_size;
  Delay e2e_latency;
  RTIMap rti_map;
  Tick phase;
  Delay jitter;
  std::string name;
  Weight weight;

  WirelessLinks wireless_links;
  DelayMap effective_release, effective_deadline;

  MessageStream(const std::shared_ptr<NetworkTopology> &network,
                const std::shared_ptr<Route> &route, Tick period,
                FrameSize frame_size, Delay e2e_latency,
                const RTIMap &rti_map = {}, Tick phase = 0, Delay jitter = 0,
                std::string name = "", Weight weight = 1)
      : network(network), route(route), period(period), frame_size(frame_size),
        e2e_latency(e2e_latency), rti_map(rti_map), phase(phase),
        jitter(jitter), name(name), weight(weight) {
    initialize();
  }

  MessageStream(const std::shared_ptr<NetworkTopology> &network, Route &&route,
                Tick period, FrameSize frame_size, Delay e2e_latency,
                const RTIMap &rti_map = {}, Tick phase = 0, Delay jitter = 0,
                std::string name = "", Weight weight = 1)
      : network(network), route(std::make_shared<Route>(std::move(route))),
        period(period), frame_size(frame_size), e2e_latency(e2e_latency),
        rti_map(rti_map), phase(phase), jitter(jitter), name(name),
        weight(weight) {
    initialize();
  }

  MessageStream(const std::shared_ptr<NetworkTopology> &network,
                const Route &route, Tick period, FrameSize frame_size,
                Delay e2e_latency, const RTIMap &rti_map = {}, Tick phase = 0,
                Delay jitter = 0, std::string name = "", Weight weight = 1)
      : network(network), route(std::make_shared<Route>(route)), period(period),
        frame_size(frame_size), e2e_latency(e2e_latency), rti_map(rti_map),
        phase(phase), jitter(jitter), name(name), weight(weight) {
    initialize();
  }

  MessageStream(const std::shared_ptr<NetworkTopology> &network,
                nlohmann::json j);

  void initialize();

  nlohmann::json dump() const;

private:
  std::shared_ptr<NetworkTopology> network;

  void compute_wired_rtis();
  void compute_effective_release(TreeRouteHop &hop, Delay release);
  Delay compute_effective_deadline(TreeRouteHop &hop);
};

void dump_streams(const std::vector<MessageStream> &streams,
                  std::filesystem::path out);

std::vector<MessageStream>
load_streams(const std::shared_ptr<NetworkTopology> &network,
             std::filesystem::path in);

} // namespace tsndgm

#endif // TSN_DGM_MESSAGE_STREAM_H
