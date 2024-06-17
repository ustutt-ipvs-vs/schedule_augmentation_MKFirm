#pragma once

#include "dgm.h"
#include "transmission_graph.h"

namespace tsndgm::critical_path {

typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;
typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

enum Objective {
  makespan,
  deadline,    // formerly fixed_lateness
  e2e_latency, // formerly dynamic_lateness
  fixed_tardiness,
  dynamic_tardiness,
};

struct Result {
  Delay objective;
  V critical_vertex;
};

inline auto get_termination_bound(const Objective type) -> Delay {
  switch (type) {
  case makespan:
    [[fallthrough]];
  case fixed_tardiness:
    [[fallthrough]];
  case dynamic_tardiness:
    return 0;
  case deadline:
    [[fallthrough]];
  case e2e_latency:
    return std::numeric_limits<Delay>::min();
  default:
    throw std::logic_error("type does not exist: " + std::to_string(type));
  }
}
void compute_longest_paths(transmission_graph_t &transmission_graph, bool reverse = true);

[[nodiscard]] auto collect_critical_path_result(const transmission_graph_t &transmission_graph, Objective type) -> Result;
[[nodiscard]] auto makespan_path(const transmission_graph_t &transmission_graph) -> Result;
[[nodiscard]] auto fixed_lateness_path(const transmission_graph_t &transmission_graph, Delay min = 0) -> Result;
[[nodiscard]] auto dynamic_lateness_path(const transmission_graph_t &transmission_graph, Delay min = 0) -> Result;

[[nodiscard]] auto get_fixed_lateness(const transmission_graph_t &transmission_graph, MessageStreamHandle ms,
                                      Edge listener) -> Delay;
[[nodiscard]] auto get_dynamic_lateness(const transmission_graph_t &transmission_graph, MessageStreamHandle ms,
                                        Edge listener) -> Delay;

auto print(const DisjunctiveGraphModel &dgm, const Result &res) -> void;
} // namespace tsndgm::critical_path
