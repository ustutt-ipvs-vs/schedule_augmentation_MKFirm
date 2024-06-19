#ifndef TSN_DGM_TSN_CONFIGURATION_H
#define TSN_DGM_TSN_CONFIGURATION_H

#include "../network/message_stream.h"
#include "../network/topology.h"
#include "dgm.h"
#include "graph_visitors.h"
#include "transmission_graph.h"

namespace tsndgm {

enum GateState { closed, open };

struct PeriodicGate {
  std::list<Delay> durations;
  GateState initial;
  Delay offset;

  PeriodicGate() : durations({}), initial(closed), offset(0){};
  PeriodicGate(std::list<Delay> durations, GateState initial = closed, Delay offset = 0)
      : durations(durations), initial(initial), offset(offset) {}
};

struct PSFPGate {
  std::list<MessageStreamHandle> streams;
  Delay open, close;

  PSFPGate(std::list<MessageStreamHandle> streams, Delay open, Delay close)
      : streams(streams), open(open), close(close) {}
};

typedef std::map<Edge, PeriodicGate> GCLConfiguration;
typedef std::map<DeviceId, std::list<PSFPGate>> PSFPConfiguration;
typedef std::map<MessageStreamHandle, Delay> InitialTransmissionConfiguration;

struct TSNConfiguration {
public:
  explicit TSNConfiguration(DisjunctiveGraphModel &dgm)
      : dgm(dgm), transmission_graph(dgm.transmission_graph), topology(dgm.network) {
    compute();
  };

  void dump(std::filesystem::path out);

  GCLConfiguration gcl_config;
  PSFPConfiguration psfp_config;
  InitialTransmissionConfiguration initial_transmission_config;

protected:
  void compute();

  DisjunctiveGraphModel &dgm;
  transmission_graph_t &transmission_graph;
  const NetworkTopology &topology;
};

class tsn_configuration_visitor final : public longest_path_visitor {
public:
  tsn_configuration_visitor(DisjunctiveGraphModel &dgm, GCLConfiguration &gcl_config, PSFPConfiguration &psfp_config)
      : longest_path_visitor(dgm), gcl_config(gcl_config), psfp_config(psfp_config), topology(dgm.network){};

  void finish_vertex(V v, const transmission_graph_t &transmission_graph);

  GCLConfiguration &gcl_config;
  PSFPConfiguration &psfp_config;
  const NetworkTopology &topology;
  std::map<Edge, Delay> last_op;
};

} // namespace tsndgm

#endif
