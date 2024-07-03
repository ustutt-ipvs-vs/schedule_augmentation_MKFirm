#include "critical_path.h"
#include "graph_visitors.h"
#include "transmission_graph.h"

namespace tsndgm::critical_path {

auto compute_longest_paths(DisjunctiveGraphModel &dgm, const bool reverse) -> void {
  const TransmissionGraphProperty &prop = dgm.transmission_graph[boost::graph_bundle];
  if (reverse) {
    reversed_dgm_traversal(dgm.transmission_graph,
                           visitor(longest_path_visitor(dgm)).root_vertex(prop.sink));
  } else {
    dgm_traversal(dgm.transmission_graph,
                  visitor(longest_path_visitor(dgm)).root_vertex(prop.src));
  }
}

auto collect_critical_path_result(const transmission_graph_t &transmission_graph, const Objective type) -> Result {
  switch (type) {
  case makespan:
    return makespan_path(transmission_graph);
  case fixed_tardiness:
    return fixed_lateness_path(transmission_graph, 0);
  case dynamic_tardiness:
    return dynamic_lateness_path(transmission_graph, 0);
  case deadline:
    return fixed_lateness_path(transmission_graph, std::numeric_limits<Delay>::min());
  case e2e_latency:
    return dynamic_lateness_path(transmission_graph, std::numeric_limits<Delay>::min());
  default:
    throw std::logic_error("type does not exist: " + std::to_string(type));
  }
}

auto makespan_path(const transmission_graph_t &transmission_graph) -> Result {
  const TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];
  return {prop.crit_cost[prop.sink], prop.sink};
}

auto fixed_lateness_path(const transmission_graph_t &transmission_graph, const Delay min) -> Result {
  const TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];
  Result max_lateness = {min, prop.src};

  for (MessageStreamHandle ms = 0; ms < prop.tt_streams.size(); ms++) {
    const auto &stream = prop.tt_streams.at(ms);
    const auto listeners = stream.route.get_listeners();

    // compute lateness of stream's end-to-end latency
    for (Edge listener : listeners) {
      const Delay lateness = get_fixed_lateness(transmission_graph, ms, listener);
      if (lateness > max_lateness.objective) {
        const V v_listener = prop.operation_to_vertex.at({listener, ms});
        max_lateness = {lateness, v_listener};
      }
    }
  }

  return max_lateness;
}

auto get_fixed_lateness(const transmission_graph_t &transmission_graph, MessageStreamHandle ms, Edge listener)
    -> Delay {
  const TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];
  const auto &stream = prop.tt_streams.at(ms);
  const V v_listener = prop.operation_to_vertex.at({listener, ms});
  const Delay lateness = prop.crit_cost[v_listener] +
                         transmission_graph[edge(v_listener, prop.sink, transmission_graph).first].weight -
                         stream.deadline;
  return lateness;
}

auto dynamic_lateness_path(const transmission_graph_t &transmission_graph, const Delay min) -> Result {
  const TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];
  Result max_lateness = {min, prop.src};

  // TODO needs new implementation
  // dgm_traversal(transmission_graph,
  //              visitor(slack_visitor(transmission_graph)).root_vertex(prop.src));

  for (MessageStreamHandle ms = 0; ms < prop.tt_streams.size(); ms++) {
    auto &stream = prop.tt_streams.at(ms);
    Edge talker = stream.route.get_talker();
    const V v_talker = prop.operation_to_vertex.at({talker, ms});

    // compute lateness of stream's end-to-end latency
    const auto listeners = stream.route.get_listeners();
    for (Edge listener : listeners) {
      const V v_listener = prop.operation_to_vertex.at({listener, ms});
      const Delay recv = prop.crit_cost[v_listener] +
                         transmission_graph[edge(v_listener, prop.sink, transmission_graph).first].weight;
      Delay lateness = recv - prop.crit_cost[v_talker] - prop.slack[v_talker] - stream.deadline;
      if (recv - stream.phase - stream.period > std::max(lateness, static_cast<Delay>(0)))
        lateness = recv - stream.phase - stream.period;
      if (lateness > max_lateness.objective)
        max_lateness = {lateness, v_listener};
    }
  }

  return max_lateness;
}

auto get_dynamic_lateness(const transmission_graph_t &transmission_graph, MessageStreamHandle ms, Edge listener)
    -> Delay {
  const TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];
  const auto &stream = prop.tt_streams.at(ms);
  Edge talker = stream.route.get_talker();
  const V v_talker = prop.operation_to_vertex.at({talker, ms});

  const V v_listener = prop.operation_to_vertex.at({listener, ms});
  const Delay recv = prop.crit_cost[v_listener] +
                     transmission_graph[edge(v_listener, prop.sink, transmission_graph).first].weight;
  Delay lateness = recv - prop.crit_cost[v_talker] - prop.slack[v_talker] - stream.deadline;
  if (recv - stream.phase - stream.period > std::max(lateness, static_cast<Delay>(0)))
    lateness = recv - stream.phase - stream.period;

  return lateness;
}

auto print(const DisjunctiveGraphModel &dgm, const Result &res) -> void {
  const TransmissionGraphProperty &prop = dgm.transmission_graph[boost::graph_bundle];
  std::cout << "Critical Path: Objective = " << res.objective << std::endl << "[hop : weight (cost)]" << std::endl;

  V v = res.critical_vertex;
  while (v != prop.src) {
    tsndgm::print(dgm.transmission_graph, dgm.network, edge(prop.crit_pred[v], v, dgm.transmission_graph).first);
    std::cout << " (" << prop.crit_cost[v] << ")" << std::endl;
    v = prop.crit_pred[v];
  }
}

} // namespace tsndgm::critical_path
