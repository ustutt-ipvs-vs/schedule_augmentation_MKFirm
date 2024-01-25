#include "neighborhood.h"
#include "../dgm/shuffle_graph.h"

namespace tsndgm {

const Neighborhood &
SelectionFullNeighborhood::compute(CriticalPath::Result res) {
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  ShuffleGraphProperty &prop =
      boost::get_property(dgm.shuffle_graph, boost::graph_bundle);

  neighborhood.clear();
  V v = res.critical_vertex, v_old;

  while (v != prop.src) {
    E e = dgm.edge(prop.crit_pred[v], v);

    if (dgm.shuffle_graph[e].edge_type == disjunctive) {
      neighborhood.flip_candidates.push_back({e});
    } else if (dgm.shuffle_graph[e].edge_type == fifo) {
      neighborhood.flip_candidates.push_back({dgm.fifo_to_disjunctive_edge(e)});
    }

    v_old = v;
    v = prop.crit_pred[v];
  }

  return neighborhood;
}

void SelectionCriticalBlockNeighborhood::critical_block_to_neighbors(
    const std::vector<V> &critical_block, CriticalBlockType type) {
  // note that critical_block[0] contains last operation of critical block
  if (critical_block.size() > 0) {
    for (int i = 1; i < critical_block.size(); i++) {
      // move critical_block[i] after critical_block[0]
      std::list<E> edges = {};
      for (int j = i - 1; j >= 0; j--)
        edges.push_back(dgm.edge(critical_block[i], critical_block[j]));
      neighborhood.flip_candidates.push_back(edges);
    }
    for (int i = 1; i < critical_block.size() - 1; i++) {
      // move critical_block[i] before critical_block[-1]
      neighborhood.flip_candidates.push_back(
          {dgm.edge(critical_block.back(), critical_block[i])});
    }
  }
}

const Neighborhood &
SelectionCriticalBlockNeighborhood::compute(CriticalPath::Result res) {
  ShuffleGraphProperty &prop =
      boost::get_property(dgm.shuffle_graph, boost::graph_bundle);
  neighborhood.clear();

  V v = res.critical_vertex, v_old;
  Edge edge = dgm.shuffle_graph[v].edge;
  std::vector<V> critical_block;
  CriticalBlockType type = last;

  while (v != prop.src) {
    V u = prop.crit_pred[v];
    E e = dgm.edge(u, v);

    if (dgm.shuffle_graph[e].edge_type != conjunctive) {
      if (edge == dgm.shuffle_graph[u].edge) {
        critical_block.push_back(u);
      } else {
        critical_block_to_neighbors(critical_block, type);
        type = intermediate;

        if (dgm.shuffle_graph[e].edge_type == disjunctive) {
          critical_block = {v, u};
        } else {
          E de = dgm.fifo_to_disjunctive_edge(e);
          critical_block = {target(de, dgm.shuffle_graph), u};
        }

        edge = dgm.shuffle_graph[u].edge;
      }
    }

    v_old = v;
    v = u;
  }
  critical_block_to_neighbors(critical_block, first);

  return neighborhood;
}

void ReducedSelectionCriticalBlockNeighborhood::critical_block_to_neighbors(
    const std::vector<V> &critical_block, CriticalBlockType type) {
  // note that critical_block[0] contains last operation of critical block
  size_t n = critical_block.size();
  if (n > 1) {
    neighborhood.flip_candidates.push_back(
        {dgm.edge(critical_block[1], critical_block[0])});
    if (n > 2) {
      neighborhood.flip_candidates.push_back(
          {dgm.edge(critical_block[n - 1], critical_block[n - 2])});
    }
  }
}

const Neighborhood &CompressionNeighborhood::compute(CriticalPath::Result res) {
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  neighborhood.clear();

  shuffle_graph_t &shuffle_graph = dgm.shuffle_graph;
  ShuffleGraphProperty &prop =
      boost::get_property(dgm.shuffle_graph, boost::graph_bundle);

  for (V v = res.critical_vertex; v != prop.src; v = prop.crit_pred[v]) {
    for (V u = prop.crit_pred[v]; u != prop.src; u = prop.crit_pred[u]) {
      auto [uv, found] = boost::edge(u, v, shuffle_graph);
      if (found && shuffle_graph[uv].edge_type != conjunctive)
        neighborhood.shuffle_candidates.push_back({uv});
    }
  }

  return neighborhood;
}

} // namespace tsndgm
