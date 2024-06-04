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
  RTI() = default;

  RTI(Delay max, Delay min = 0, Delay prop = 0, Delay proc = 0)
      : min(min), max(max), prop(prop), proc(proc) {}

  RTI(FrameSize frame_size, DataRate data_rate, Delay prop = 0, Delay proc = 0,
      Delay skew = 0)
      : max(frame_size * SECONDS_TO_TICKS(1) / data_rate + skew),
        min(frame_size * SECONDS_TO_TICKS(1) / data_rate - skew), prop(prop),
        proc(proc) {}

  Delay d_max() const { return max + prop + proc; }
  Delay d_min() const { return min + proc; }
  Delay d_trans_max() const { return max + prop; }
  Delay d_trans_min() const { return min; }

private:
  Delay max;
  Delay min;
  Delay prop;
  Delay proc;
};

typedef std::map<Edge, RTI> RTIMap;
typedef std::map<Edge, Delay> DelayMap;
typedef std::list<Edge> WirelessLinks;

typedef unsigned int MessageStreamHandle;

class MessageStream {
public:
  std::shared_ptr<Route> route;

  Tick period;
  FrameSize frame_size;
  Delay e2e_latency; // TODO cinsider updating this field to handle deadlines
                     // properly
  RTIMap rti_map;
  Tick phase;
  Delay jitter;
  std::string name;

  MessageStream(const std::shared_ptr<NetworkTopology> &network,
                const std::shared_ptr<Route> &route, Tick period,
                FrameSize frame_size, Delay e2e_latency,
                const RTIMap &rti_map = {}, Tick phase = 0, Delay jitter = 0,
                std::string name = "")
      : network(network), route(route), period(period), frame_size(frame_size),
        e2e_latency(e2e_latency), rti_map(rti_map), phase(phase),
        jitter(jitter), name(name) {
    initialize();
  }

  MessageStream(const std::shared_ptr<NetworkTopology> &network, Route &&route,
                Tick period, FrameSize frame_size, Delay e2e_latency,
                const RTIMap &rti_map = {}, Tick phase = 0, Delay jitter = 0,
                std::string name = "")
      : network(network), route(std::make_shared<Route>(std::move(route))),
        period(period), frame_size(frame_size), e2e_latency(e2e_latency),
        rti_map(rti_map), phase(phase), jitter(jitter), name(name) {
    initialize();
  }

  MessageStream(const std::shared_ptr<NetworkTopology> &network,
                const Route &route, Tick period, FrameSize frame_size,
                Delay e2e_latency, const RTIMap &rti_map = {}, Tick phase = 0,
                Delay jitter = 0, std::string name = "")
      : network(network), route(std::make_shared<Route>(route)), period(period),
        frame_size(frame_size), e2e_latency(e2e_latency), rti_map(rti_map),
        phase(phase), jitter(jitter), name(name) {
    initialize();
  }

  MessageStream(const std::shared_ptr<NetworkTopology> &network,
                nlohmann::json j);

  void initialize();

  nlohmann::json dump() const;

private:
  std::shared_ptr<NetworkTopology> network;

  void compute_wired_rtis();
};

void dump_streams(const std::vector<MessageStream> &streams,
                  std::filesystem::path out);

std::vector<MessageStream>
load_streams(const std::shared_ptr<NetworkTopology> &network,
             std::filesystem::path in);
} // namespace tsndgm

#endif // TSN_DGM_MESSAGE_STREAM_H
