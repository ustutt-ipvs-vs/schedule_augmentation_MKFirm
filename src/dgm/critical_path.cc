#include "critical_path.h"
#include "transmission_graph.h"

namespace tsndgm {
int longest_path_visitor::total_traversals = 0;

void CriticalPath::compute_longest_paths(bool reverse) {
  TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];
  if (reverse) {
    reversed_dgm_traversal(transmission_graph,
                           visitor(longest_path_visitor(transmission_graph))
                               .root_vertex(prop.sink));
  } else {
    dgm_traversal(transmission_graph,
                  visitor(longest_path_visitor(transmission_graph))
                      .root_vertex(prop.src));
  }
}

CriticalPath::Result CriticalPath::path(CriticalPath::Objective type) {
  switch (type) {
  case makespan:
    return makespan_path();
  case fixed_tardiness:
    return fixed_lateness_path(0);
  case dynamic_tardiness:
    return dynamic_lateness_path(0);
  case fixed_lateness:
    return fixed_lateness_path(std::numeric_limits<Delay>::min());
  case dynamic_lateness:
    return dynamic_lateness_path(std::numeric_limits<Delay>::min());
  default:
    throw std::logic_error("type does not exist: " + std::to_string(type));
  }
}

CriticalPath::Result CriticalPath::makespan_path() {
  TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];
  return {prop.crit_cost[prop.sink], prop.sink};
}

CriticalPath::Result CriticalPath::fixed_lateness_path(Delay min) {
  TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];
  Result max_lateness = {min, prop.src};

  for (MessageStreamHandle ms = 0; ms < prop.streams.size(); ms++) {
    auto &stream = prop.streams[ms];
    const std::list<Edge> &listeners = stream.route->get_listeners();

    // compute lateness of stream's end-to-end latency
    for (Edge listener : listeners) {
      Delay lateness = get_fixed_lateness(ms, listener);
      if (lateness > max_lateness.objective) {
        V v_listener = prop.operation_to_vertex[{listener, ms}];
        max_lateness = {lateness, v_listener};
      }
    }
  }

  return max_lateness;
}

Delay CriticalPath::get_fixed_lateness(MessageStreamHandle ms, Edge listener) {
  TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];
  auto &stream = prop.streams[ms];
  V v_listener = prop.operation_to_vertex[{listener, ms}];
  Delay lateness =
      prop.crit_cost[v_listener] +
      transmission_graph[boost::edge(v_listener, prop.sink, transmission_graph)
                             .first]
          .weight -
      stream.e2e_latency;
  return lateness;
}

CriticalPath::Result CriticalPath::dynamic_lateness_path(Delay min) {
  TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];
  Result max_lateness = {min, prop.src};

  // TODO needs new implementation
  // dgm_traversal(transmission_graph,
  //              visitor(slack_visitor(transmission_graph)).root_vertex(prop.src));

  for (MessageStreamHandle ms = 0; ms < prop.streams.size(); ms++) {
    auto &stream = prop.streams[ms];
    Edge talker = stream.route->get_talker();
    V v_talker = prop.operation_to_vertex[{talker, ms}];

    // compute lateness of stream's end-to-end latency
    const std::list<Edge> &listeners = stream.route->get_listeners();
    for (Edge listener : listeners) {
      V v_listener = prop.operation_to_vertex[{listener, ms}];
      Delay recv =
          prop.crit_cost[v_listener] +
          transmission_graph
              [boost::edge(v_listener, prop.sink, transmission_graph).first]
                  .weight;
      Delay lateness = recv - prop.crit_cost[v_talker] - prop.slack[v_talker] -
                       stream.e2e_latency;
      if (recv - stream.phase - stream.period > std::max(lateness, (Delay)0))
        lateness = recv - stream.phase - stream.period;
      if (lateness > max_lateness.objective)
        max_lateness = {lateness, v_listener};
    }
  }

  return max_lateness;
}

Delay CriticalPath::get_dynamic_lateness(MessageStreamHandle ms,
                                         Edge listener) {
  TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];
  auto &stream = prop.streams[ms];
  Edge talker = stream.route->get_talker();
  V v_talker = prop.operation_to_vertex[{talker, ms}];

  V v_listener = prop.operation_to_vertex[{listener, ms}];
  Delay recv =
      prop.crit_cost[v_listener] +
      transmission_graph[boost::edge(v_listener, prop.sink, transmission_graph)
                             .first]
          .weight;
  Delay lateness = recv - prop.crit_cost[v_talker] - prop.slack[v_talker] -
                   stream.e2e_latency;
  if (recv - stream.phase - stream.period > std::max(lateness, (Delay)0))
    lateness = recv - stream.phase - stream.period;

  return lateness;
}

void CriticalPath::print(Result res, const NetworkTopology &network) {
  TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];
  std::cout << "Critical Path: Objective = " << res.objective << std::endl
            << "[hop : weight (cost)]" << std::endl;

  V v = res.critical_vertex;
  while (v != prop.src) {
    tsndgm::print(transmission_graph, network,
                  boost::edge(prop.crit_pred[v], v, transmission_graph).first);
    std::cout << " (" << prop.crit_cost[v] << ")" << std::endl;
    v = prop.crit_pred[v];
  }
}
} // namespace tsndgm
