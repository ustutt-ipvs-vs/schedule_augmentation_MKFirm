#ifndef TSN_DGM_GCL_H
#define TSN_DGM_GCL_H

#include "../network/message_stream.h"
#include "../network/topology.h"

namespace tsndgm {

typedef std::map<MessageStreamHandle, Delay> ReleaseTimes;

enum GateState { closed, open };
struct GCLEntry {
  PCPValue queues;
  std::map<Delay, std::vector<GateState>> gcl;

  inline const std::pair<Delay, std::vector<GateState>>
  operator[](Delay t) const {
    return *(gcl.upper_bound(t)--);
  }

  Delay next_transmission_opportunity(Delay t, PCPValue pcp,
                                      Delay d_trans_max) {
    auto it = std::prev(gcl.upper_bound(t));
    Delay slack = std::next(it)->first - t;
    while (it->second[pcp] == closed || d_trans_max > slack) {
      it++;
      slack = std::next(it)->first - it->first;
    }
    return it->first;
  }
};

typedef std::map<Edge, GCLEntry> GCLConfiguration;

struct PSFPEnforcedInterval {
  std::map<Delay, Delay> intervals;

  bool passed(Delay t) {
    auto it = intervals.lower_bound(t);
    return it->first <= t && t < it->second;
  }

  bool passed(Delay lower, Delay upper) {
    auto it = intervals.lower_bound(lower);
    return it->first <= lower && upper < it->second;
  }
};

struct PSFPEntry {
  std::map<MessageStreamHandle, PSFPEnforcedInterval> psfp_enforced_intervals;

  inline const PSFPEnforcedInterval &operator[](MessageStreamHandle ms) {
    return psfp_enforced_intervals[ms];
  }
};

typedef std::map<DeviceId, PSFPEntry> PSFPConfiguration;

struct TSNConfiguration {
  ReleaseTimes release_times;
  GCLConfiguration gcl_configuration;
  PSFPConfiguration psfp_configuration;
};

} // namespace tsndgm

#endif
