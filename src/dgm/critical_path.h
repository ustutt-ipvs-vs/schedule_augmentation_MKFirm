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

  void examine_edge(E e, const shuffle_graph_t &shuffle_graph) const {
    assert((shuffle_graph[e].state() == allowed));
  }

  void discover_vertex(V v, const shuffle_graph_t &shuffle_graph) const {
    prop.crit_cost[v] = 0;
    if (reversed) {
      prop.crit_pred[v] = prop.src;
    } else {
      prop.crit_pred[v] = prop.sink;
    }
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

class update_machine_successors_visitor : public longest_path_visitor {
public:
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;

  update_machine_successors_visitor(shuffle_graph_t &shuffle_graph,
                                    std::map<V, V> &updated_machine_successors)
      : longest_path_visitor(shuffle_graph),
        updated_machine_successors(updated_machine_successors) {}

  void discover_vertex(V v, const shuffle_graph_t &shuffle_graph) const {
    longest_path_visitor::discover_vertex(v, shuffle_graph);

    if (!shuffle_graph[v].neighbors_are_valid) {
      auto find = updated_machine_successors.find(v);
      if (find == updated_machine_successors.end() ||
          ((*find).second != 0 &&
           shuffle_graph[boost::edge(v, (*find).second, shuffle_graph).first]
                   .state() == blocked))
        updated_machine_successors[v] = 0;
    }
  }

  void examine_edge(E e, const shuffle_graph_t &shuffle_graph) const {
    assert((shuffle_graph[e].state() == allowed));

    if (shuffle_graph[e].edge_type != disjunctive)
      return;

    V u = source(e, shuffle_graph), v = target(e, shuffle_graph);
    if (shuffle_graph[u].neighbors_are_valid)
      return;

    auto find = updated_machine_successors.find(u);
    if (find == updated_machine_successors.end() || (*find).second == 0 ||
        (*find).second == v ||
        shuffle_graph[boost::edge((*find).second, v, shuffle_graph).first]
                .state() == blocked ||
        shuffle_graph[boost::edge(u, (*find).second, shuffle_graph).first]
                .state() == blocked) {
      updated_machine_successors[u] = v;
    }
  }

  std::map<V, V> &updated_machine_successors;
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

  void print(Result res, const NetworkTopology &network);

private:
  shuffle_graph_t &shuffle_graph;
};

} // namespace tsndgm

#endif // TSN_DGM_CRITICAL_PATH_H
