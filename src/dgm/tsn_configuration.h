#ifndef TSN_DGM_TSN_CONFIGURATION_H
#define TSN_DGM_TSN_CONFIGURATION_H

#include "../network/message_stream.h"
#include "../network/topology.h"
#include "critical_path.h"
#include "transmission_graph.h"

namespace tsndgm {

enum GateState { closed, open };

struct PeriodicGate {
  std::list<Delay> durations;
  GateState initial;
  Delay offset;

  PeriodicGate() : durations({}), initial(closed), offset(0){};
  PeriodicGate(std::list<Delay> durations, GateState initial = closed,
               Delay offset = 0)
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
  TSNConfiguration(transmission_graph_t &transmission_graph, NetworkTopology &topology)
      : transmission_graph(transmission_graph), topology(topology) {
    compute();
  };

  void dump(std::filesystem::path out);

  GCLConfiguration gcl_config;
  PSFPConfiguration psfp_config;
  InitialTransmissionConfiguration initial_transmission_config;

protected:
  void compute();

  transmission_graph_t &transmission_graph;
  NetworkTopology &topology;
};

class tsn_configuration_visitor : public longest_path_visitor {
public:
  tsn_configuration_visitor(transmission_graph_t &transmission_graph,
                            NetworkTopology &topology,
                            GCLConfiguration &gcl_config,
                            PSFPConfiguration &psfp_config)
      : longest_path_visitor(transmission_graph), topology(topology),
        gcl_config(gcl_config), psfp_config(psfp_config){};

  void finish_vertex(V v, const transmission_graph_t &transmission_graph);

  GCLConfiguration &gcl_config;
  PSFPConfiguration &psfp_config;
  const NetworkTopology &topology;
  std::map<Edge, Delay> last_op;
};

} // namespace tsndgm

#endif
