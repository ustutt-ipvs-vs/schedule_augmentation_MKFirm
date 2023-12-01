#include "critical_path.h"
#include "shuffle_graph.h"

namespace tsndgm {

void CriticalPath::compute_longest_paths() {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  reversed_dgm_traversal(
      shuffle_graph,
      visitor(longest_path_visitor(shuffle_graph)).root_vertex(prop.sink));
}

CriticalPath::Result CriticalPath::path(CriticalPath::Objective type) {
  switch (type) {
  case makespan:
    return makespan_path();
  case tardiness:
    return tardiness_path();
  default:
    throw std::logic_error("type does not exist: " + std::to_string(type));
  }
}

CriticalPath::Result CriticalPath::makespan_path() {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  return {prop.crit_cost[prop.sink], prop.sink};
}

CriticalPath::Result CriticalPath::tardiness_path() {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  std::pair<V, Delay> max_tardiness = std::make_pair(prop.src, 0);

  for (auto ed :
       boost::make_iterator_range(boost::out_edges(prop.src, shuffle_graph))) {
    V u = boost::target(ed, shuffle_graph);
    for (auto handle : shuffle_graph[u].ms_handle) {
      Delay tardiness = prop.crit_cost[u] - prop.streams[handle].phase -
                        prop.streams[handle].period;
      if (tardiness > max_tardiness.second)
        max_tardiness = std::make_pair(u, tardiness);
    }
  }

  std::list<MessageStreamHandle>::const_iterator it1;
  std::list<V>::const_iterator it2;
  for (auto ed :
       boost::make_iterator_range(boost::in_edges(prop.sink, shuffle_graph))) {
    V u = boost::source(ed, shuffle_graph);
    for (it1 = shuffle_graph[u].ms_handle.begin(),
        it2 = shuffle_graph[u].root.begin();
         it1 != shuffle_graph[u].ms_handle.end() &&
         it2 != shuffle_graph[u].root.end();
         ++it1, ++it2) {
      Delay tardiness = prop.crit_cost[u] + shuffle_graph[ed].weight -
                        prop.crit_cost[*it2] - prop.streams[*it1].e2e_latency;
      if (tardiness > max_tardiness.second)
        max_tardiness = std::make_pair(u, tardiness);
    }
  }

  return {max_tardiness.second, max_tardiness.first};
}

void CriticalPath::print(Result res) {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  std::cout << "Critical Path: Objective = " << res.objective << std::endl
            << "[hop : weight (cost)]" << std::endl;

  V v = res.critical_vertex;
  while (v != prop.src) {
    tsndgm::print(shuffle_graph, prop,
                  boost::edge(prop.crit_pred[v], v, shuffle_graph).first);
    std::cout << " (" << prop.crit_cost[v] << ")" << std::endl;
    v = prop.crit_pred[v];
  }
}

} // namespace tsndgm
