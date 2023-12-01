#ifndef TSN_DGM_CRITICAL_PATH_H
#define TSN_DGM_CRITICAL_PATH_H

#include "shuffle_graph.h"
#include "traversal.h"

namespace tsndgm {

class longest_path_visitor : public boost::default_dfs_visitor {
public:
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  longest_path_visitor(shuffle_graph_t &shuffle_graph)
      : prop(boost::get_property(shuffle_graph, boost::graph_bundle)) {}

  void back_edge(E e, const shuffle_graph_t &shuffle_graph) const {
    throw std::runtime_error(
        "Selection is not complete; disjunctive graph is acyclic.");
  }

  void discover_vertex(V v, const shuffle_graph_t &shuffle_graph) const {
    prop.crit_cost[v] = 0;
    prop.crit_pred[v] = prop.src;
  }

  void finish_edge(E e, const shuffle_graph_t &shuffle_graph) const {
    Delay v_cost = prop.crit_cost[target(e, shuffle_graph)];
    Delay u_cost =
        prop.crit_cost[source(e, shuffle_graph)] + shuffle_graph[e].weight;
    if (u_cost > v_cost) {
      prop.crit_cost[target(e, shuffle_graph)] = u_cost;
      prop.crit_pred[target(e, shuffle_graph)] = source(e, shuffle_graph);
    }
  }

  ShuffleGraphProperty &prop;
};

class CriticalPath {
public:
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  enum Objective { makespan, tardiness };

  struct Result {
    Delay objective;
    V critical_vertex;
  };

  CriticalPath(shuffle_graph_t &shuffle_graph) : shuffle_graph(shuffle_graph) {}

  void compute_longest_paths();

  Result path(Objective type);
  Result makespan_path();
  Result tardiness_path();

  void print(Result res);

private:
  shuffle_graph_t &shuffle_graph;
};

} // namespace tsndgm

#endif // TSN_DGM_CRITICAL_PATH_H
