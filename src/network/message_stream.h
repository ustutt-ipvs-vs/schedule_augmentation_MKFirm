#ifndef TSN_DGM_MESSAGE_STREAM_H
#define TSN_DGM_MESSAGE_STREAM_H

#include "route.h"
#include "topology.h"

namespace tsndgm {

typedef unsigned int FrameSize;
typedef unsigned short PCPValue;

class RTI {
public:
  RTI(){};
  RTI(Delay max, Delay min = 0, Delay prop = 0)
      : min(min), max(max), prop(prop) {}

  Delay d_max() const { return max + prop; }
  Delay d_min() const { return min + prop; }
  Delay d_trans_max() const { return max; }
  Delay d_trans_min() const { return min; }

private:
  Delay max;
  Delay min;
  Delay prop;
};

static RTI WIRED_RTI(FrameSize frame_size, DataRate data_rate, Delay prop = 0) {
  return RTI(frame_size * SECONDS_TO_TICKS(1) / data_rate,
             frame_size * SECONDS_TO_TICKS(1) / data_rate, prop);
}

typedef std::map<Edge, RTI> RTIMap;
typedef std::map<Edge, Delay> DelayMap;
typedef std::list<Edge> WirelessLinks;

typedef unsigned int MessageStreamHandle;

class MessageStream {
public:
  std::shared_ptr<Route> route;
  RTIMap rti_map;

  Tick period;
  Tick phase;
  FrameSize frame_size;

  Delay e2e_latency;
  Delay jitter;
  PCPValue pcp;

  WirelessLinks wireless_links;
  DelayMap effective_release, effective_deadline;

  MessageStream(const std::shared_ptr<NetworkTopology> &network,
                const std::shared_ptr<Route> &route, Tick period,
                FrameSize frame_size, Delay e2e_latency,
                const RTIMap &rti_map = {}, Tick phase = 0, Delay jitter = 0,
                PCPValue pcp = 0)
      : network(network), route(route), period(period), frame_size(frame_size),
        e2e_latency(e2e_latency), rti_map(rti_map), phase(phase),
        jitter(jitter), pcp(pcp) {
    compute_wired_rtis();
    compute_effective_deadline(route->root.childs.front());
    compute_effective_release(route->root.childs.front(), phase);
  }

  MessageStream(const std::shared_ptr<NetworkTopology> &network, Route &&route,
                Tick period, FrameSize frame_size, Delay e2e_latency,
                const RTIMap &rti_map = {}, Tick phase = 0, Delay jitter = 0,
                PCPValue pcp = 0)
      : network(network), route(std::make_shared<Route>(std::move(route))),
        period(period), frame_size(frame_size), e2e_latency(e2e_latency),
        rti_map(rti_map), phase(phase), jitter(jitter), pcp(pcp) {
    compute_wired_rtis();
    compute_effective_deadline(this->route->root.childs.front());
    compute_effective_release(this->route->root.childs.front(), phase);
  }

  MessageStream(const std::shared_ptr<NetworkTopology> &network,
                const Route &route, Tick period, FrameSize frame_size,
                Delay e2e_latency, const RTIMap &rti_map = {}, Tick phase = 0,
                Delay jitter = 0, PCPValue pcp = 0)
      : network(network), route(std::make_shared<Route>(route)), period(period),
        frame_size(frame_size), e2e_latency(e2e_latency), rti_map(rti_map),
        phase(phase), jitter(jitter), pcp(pcp) {
    compute_wired_rtis();
    compute_effective_deadline(this->route->root.childs.front());
    compute_effective_release(this->route->root.childs.front(), phase);
  }

private:
  std::shared_ptr<NetworkTopology> network;

  void compute_wired_rtis();
  void compute_effective_release(TreeRouteHop &hop, Delay release);
  Delay compute_effective_deadline(TreeRouteHop &hop);
};

} // namespace tsndgm

#endif // TSN_DGM_MESSAGE_STREAM_H
