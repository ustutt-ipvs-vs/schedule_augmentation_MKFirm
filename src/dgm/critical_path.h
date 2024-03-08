#ifndef TSN_DGM_CRITICAL_PATH_H
#define TSN_DGM_CRITICAL_PATH_H

#include "shuffle_graph.h"
#include "traversal.h"

namespace tsndgm {

class longest_path_visitor : public boost::default_dfs_visitor {
public:
  static int total_traversals;
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  longest_path_visitor(shuffle_graph_t &shuffle_graph)
      : prop(boost::get_property(shuffle_graph, boost::graph_bundle)) {
    total_traversals++;
  }

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

class feasibility_visitor : public update_machine_successors_visitor {
public:
  feasibility_visitor(shuffle_graph_t &shuffle_graph,
                      std::map<V, V> &updated_machine_successors,
                      bool &feasible)
      : update_machine_successors_visitor(shuffle_graph,
                                          updated_machine_successors),
        feasible(feasible) {}

  bool back_edge(E e, const shuffle_graph_t &shuffle_graph) {
    feasible = false;
    return true; // aborts traversal
  }

private:
  bool &feasible;
};

class slack_visitor : public boost::default_dfs_visitor {
public:
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  slack_visitor(shuffle_graph_t &shuffle_graph)
      : prop(boost::get_property(shuffle_graph, boost::graph_bundle)) {
    longest_path_visitor::total_traversals++;
  }

  virtual bool back_edge(E e, const shuffle_graph_t &shuffle_graph) const {
    throw std::runtime_error(
        "Selection is not complete; disjunctive graph is acyclic.");
  }

  void examine_edge(E e, const shuffle_graph_t &shuffle_graph) const {
    assert((shuffle_graph[e].state() == allowed));
  }

  void discover_vertex(V v, const shuffle_graph_t &shuffle_graph) const {
    if (v == prop.sink)
      prop.slack[v] = 0;
    else
      prop.slack[v] = std::numeric_limits<Delay>::max();
  }

  void finish_edge(E uv, const shuffle_graph_t &shuffle_graph) const {
    V u = source(uv, shuffle_graph), v = target(uv, shuffle_graph);

    if (shuffle_graph[uv].weight == std::numeric_limits<Delay>::min())
      return;

    Delay uv_slack =
        prop.crit_cost[v] - prop.crit_cost[u] - shuffle_graph[uv].weight;
    if (uv_slack + prop.slack[v] < prop.slack[u])
      prop.slack[u] = uv_slack + prop.slack[v];
  }

  ShuffleGraphProperty &prop;
  bool reversed = false;
};

class CriticalPath {
public:
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  enum Objective {
    makespan,
    fixed_tardiness,
    dynamic_tardiness,
    fixed_lateness,
    dynamic_lateness
  };

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

  static Delay get_termination_bound(Objective type) {
    switch (type) {
    case makespan:
    case fixed_tardiness:
    case dynamic_tardiness:
      return 0;
    case fixed_lateness:
    case dynamic_lateness:
      return std::numeric_limits<Delay>::min();
    default:
      throw std::logic_error("type does not exist: " + std::to_string(type));
    }
  }

  void compute_longest_paths(bool reverse = true);

  Result path(Objective type);
  Result makespan_path();
  Result fixed_tardiness_path(Delay min = 0);
  Result dynamic_tardiness_path(Delay min = 0);

  void print(Result res, const NetworkTopology &network);

private:
  shuffle_graph_t &shuffle_graph;
};

} // namespace tsndgm

#endif // TSN_DGM_CRITICAL_PATH_H
