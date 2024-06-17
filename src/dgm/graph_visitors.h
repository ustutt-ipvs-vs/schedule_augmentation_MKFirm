#pragma once

#include "transmission_graph.h"
#include "traversal.h"

namespace tsndgm {

class longest_path_visitor : public boost::default_dfs_visitor {
public:
  virtual ~longest_path_visitor() = default;
  static int total_traversals;
  typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

  explicit longest_path_visitor(transmission_graph_t &transmission_graph)
      : prop(get_property(transmission_graph, boost::graph_bundle)) {
    total_traversals++;
  }

  [[nodiscard]] virtual bool back_edge(E e, const transmission_graph_t &transmission_graph) const {
    throw std::runtime_error("Selection is not complete; disjunctive graph is acyclic.");
  }

  void discover_vertex(V v, const transmission_graph_t &transmission_graph) const {
    prop.crit_cost[v] = 0;
    if (reversed) {
      prop.crit_pred[v] = prop.src;
    } else {
      prop.crit_pred[v] = prop.sink;
    }
  }

  void finish_edge(E uv, const transmission_graph_t &transmission_graph) const {
    V u, v;
    if (reversed) {
      u = source(uv, transmission_graph), v = target(uv, transmission_graph);
    } else {
      v = source(uv, transmission_graph), u = target(uv, transmission_graph);
    }
    const Delay v_cost = prop.crit_cost[v];
    const Delay u_cost = prop.crit_cost[u] + transmission_graph[uv].weight;
    if (u_cost >= v_cost) {
      prop.crit_cost[v] = u_cost;
      prop.crit_pred[v] = u;
    }
  }

  TransmissionGraphProperty &prop;
  bool reversed = true;
};

class slack_visitor final : public boost::default_dfs_visitor {
public:
  virtual ~slack_visitor() = default;
  typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

  explicit slack_visitor(transmission_graph_t &transmission_graph)
      : prop(get_property(transmission_graph, boost::graph_bundle)) {
    longest_path_visitor::total_traversals++;
  }

  [[nodiscard]] virtual bool back_edge(E e, const transmission_graph_t &transmission_graph) const {
    throw std::runtime_error("Selection is not complete; disjunctive graph is acyclic.");
  }

  void discover_vertex(V v, const transmission_graph_t &transmission_graph) const {
    if (v == prop.sink || boost::edge(v, prop.sink, transmission_graph).second)
      prop.slack[v] = 0;
    else
      prop.slack[v] = std::numeric_limits<Delay>::max();
  }

  void finish_edge(E uv, const transmission_graph_t &transmission_graph) const {
    V u = source(uv, transmission_graph), v = target(uv, transmission_graph);

    if (transmission_graph[uv].weight == std::numeric_limits<Delay>::min())
      return;

    Delay uv_slack = prop.crit_cost[v] - prop.crit_cost[u] - transmission_graph[uv].weight;
    if (uv_slack + prop.slack[v] < prop.slack[u])
      prop.slack[u] = uv_slack + prop.slack[v];
  }

  TransmissionGraphProperty &prop;
  bool reversed = false;
};

} // namespace tsndgm
