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

  virtual bool back_edge(E e, const shuffle_graph_t &shuffle_graph) const {
    throw std::runtime_error(
        "Selection is not complete; disjunctive graph is acyclic.");
  }

  void discover_vertex(V v, const shuffle_graph_t &shuffle_graph) const {
    prop.crit_cost[v] = 0;
    prop.crit_pred[v] = prop.src;
  }

  void finish_edge(E uv, const shuffle_graph_t &shuffle_graph) const {
    V u, v;
    if (reversed) {
      u = source(uv, shuffle_graph), v = target(uv, shuffle_graph);
    } else {
      v = source(uv, shuffle_graph), u = target(uv, shuffle_graph);
    }
    Delay v_cost = prop.crit_cost[v];
    Delay u_cost = prop.crit_cost[u] + shuffle_graph[uv].weight;
    if (u_cost >= v_cost) {
      prop.crit_cost[v] = u_cost;
      prop.crit_pred[v] = u;
    }
  }

  ShuffleGraphProperty &prop;
  bool reversed = true;
};

class feasibility_visitor : public longest_path_visitor {
public:
  feasibility_visitor(shuffle_graph_t &shuffle_graph, bool &feasible)
      : longest_path_visitor(shuffle_graph), feasible(feasible) {}

  bool back_edge(E e, const shuffle_graph_t &shuffle_graph) {
    feasible = false;
    return true; // aborts traversal
  }

private:
  bool &feasible;
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

  CriticalPath &operator=(const CriticalPath &other) {
    if (this != &other) {
      shuffle_graph = other.shuffle_graph;
    }
    return *this;
  }

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
