#ifndef TSN_DGM_CRITICAL_PATH_H
#define TSN_DGM_CRITICAL_PATH_H

#include "shuffle_graph.h"
#include "traversal.h"

namespace tsndgm {

template <typename CostMap, typename PredecessorMap, typename Vertex>
class longest_path_visitor : public boost::default_dfs_visitor {
  typedef typename boost::property_traits<CostMap>::value_type T;

public:
  longest_path_visitor(CostMap cost, PredecessorMap pred, Vertex src)
      : cost(cost), pred(pred), src(src) {}

  template <typename Edge, typename Graph>
  void back_edge(Edge e, const Graph &g) const {
    throw std::runtime_error(
        "Selection is not complete; disjunctive graph is acyclic.");
  }

  template <typename Graph>
  void discover_vertex(Vertex v, const Graph &g) const {
    boost::put(cost, v, 0);
    boost::put(pred, v, src);
  }

  template <typename Edge, typename Graph>
  void finish_edge(Edge e, const Graph &g) const {
    T v_cost = boost::get(cost, target(e, g));
    T u_cost = boost::get(cost, source(e, g)) + g[e].weight;
    if (u_cost > v_cost) {
      boost::put(cost, target(e, g), u_cost);
      boost::put(pred, target(e, g), source(e, g));
    }
  }

  CostMap cost;
  PredecessorMap pred;
  Vertex src;
};

template <typename CostMap, typename PredecessorMap, typename Vertex>
longest_path_visitor<CostMap, PredecessorMap, Vertex>
make_longest_path_visitor(CostMap cost, PredecessorMap pred, Vertex src) {
  return longest_path_visitor<CostMap, PredecessorMap, Vertex>(cost, pred, src);
}

class CriticalPath {
public:
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  struct Result {
    Delay objective;
    V critical_vertex;
  };

  CriticalPath(shuffle_graph_t &shuffle_graph)
      : shuffle_graph(shuffle_graph),
        prop(boost::get_property(shuffle_graph, boost::graph_bundle)) {}

  void compute_longest_paths();

  Result makespan_path();
  Result tardiness_path();

  void print(Result res);

private:
  shuffle_graph_t &shuffle_graph;
  ShuffleGraphProperty &prop;

  std::list<E> candidates;

  std::vector<V> pred;
  std::vector<Delay> cost;
};

} // namespace tsndgm

// V v = prop.sink;
// while (v != prop.src) {
//   E e = boost::edge(pred[v], v, shuffle_graph).first;
//   if (shuffle_graph[e].edge_type != conjunctive)
//     candidates.push_front(e);
//   v = pred[v];
// }

#endif // TSN_DGM_CRITICAL_PATH_H
