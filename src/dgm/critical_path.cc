#include "critical_path.h"
#include "shuffle_graph.h"

namespace tsndgm {

void CriticalPath::compute_longest_paths(bool reverse) {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  if (reverse) {
    reversed_dgm_traversal(
        shuffle_graph,
        visitor(longest_path_visitor(shuffle_graph)).root_vertex(prop.sink));
  } else {
    dgm_traversal(
        shuffle_graph,
        visitor(longest_path_visitor(shuffle_graph)).root_vertex(prop.src));
  }
}

CriticalPath::Result CriticalPath::path(CriticalPath::Objective type) {
  switch (type) {
  case makespan:
    return makespan_path();
  case fixed_tardiness:
    return fixed_tardiness_path();
  case dynamic_tardiness:
    return dynamic_tardiness_path();
  default:
    throw std::logic_error("type does not exist: " + std::to_string(type));
  }
}

CriticalPath::Result CriticalPath::makespan_path() {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  return {prop.crit_cost[prop.sink], prop.sink};
}

CriticalPath::Result CriticalPath::fixed_tardiness_path() {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  std::pair<V, Delay> max_tardiness = std::make_pair(prop.src, 0);

  for (MessageStreamHandle ms = 0; ms < prop.streams.size(); ms++) {
    auto &stream = prop.streams[ms];
    const std::list<Edge> &listeners = stream.route->get_listeners();

    // compute tardiness of stream's end-to-end latency
    for (Edge listener : listeners) {
      V v_listener = prop.operation_to_vertex[{listener, ms}];
      Delay tardiness = prop.crit_cost[v_listener] +
                        stream.rti_map[listener].d_max() - stream.e2e_latency -
                        stream.phase;
      if (tardiness > max_tardiness.second)
        max_tardiness = std::make_pair(v_listener, tardiness);
    }
  }

  return {max_tardiness.second, max_tardiness.first};
}

CriticalPath::Result CriticalPath::dynamic_tardiness_path() {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  std::pair<V, Delay> max_tardiness = std::make_pair(prop.src, 0);

  dgm_traversal(shuffle_graph,
                visitor(slack_visitor(shuffle_graph)).root_vertex(prop.src));

  for (MessageStreamHandle ms = 0; ms < prop.streams.size(); ms++) {
    auto &stream = prop.streams[ms];
    Edge talker = stream.route->get_talker();
    const std::list<Edge> &listeners = stream.route->get_listeners();

    // compute tardiness of stream's release
    V v_talker = prop.operation_to_vertex[{talker, ms}];
    Delay release_tardiness =
        prop.crit_cost[v_talker] - stream.phase - stream.period;
    if (release_tardiness > max_tardiness.second)
      max_tardiness = std::make_pair(v_talker, release_tardiness);

    Delay slack = 0;
    if (release_tardiness < 0)
      slack = std::min(-release_tardiness, prop.slack[v_talker]);

    // compute tardiness of stream's end-to-end latency
    for (Edge listener : listeners) {
      V v_listener = prop.operation_to_vertex[{listener, ms}];
      Delay tardiness = prop.crit_cost[v_listener] +
                        stream.rti_map[listener].d_max() -
                        prop.crit_cost[v_talker] - slack - stream.e2e_latency;
      if (tardiness > max_tardiness.second)
        max_tardiness = std::make_pair(v_listener, tardiness);
    }
  }

  return {max_tardiness.second, max_tardiness.first};
}

void CriticalPath::print(Result res, const NetworkTopology &network) {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  std::cout << "Critical Path: Objective = " << res.objective << std::endl
            << "[hop : weight (cost)]" << std::endl;

  V v = res.critical_vertex;
  while (v != prop.src) {
    tsndgm::print(shuffle_graph, network,
                  boost::edge(prop.crit_pred[v], v, shuffle_graph).first);
    std::cout << " (" << prop.crit_cost[v] << ")" << std::endl;
    v = prop.crit_pred[v];
  }
}

} // namespace tsndgm
