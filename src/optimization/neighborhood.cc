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

    // only by flipping disjunctive and FIFO edges, or merging FIFO edges
    // we can potentially reduce the objective
    if (dgm.shuffle_graph[e].edge_type == disjunctive) {
      neighborhood.flip_candidates.push_back({e});
    } else if (dgm.shuffle_graph[e].edge_type == fifo) {
      neighborhood.flip_candidates.push_back(
          {dgm.edge(prop.crit_pred[v], v_old)});
      neighborhood.shuffle_candidates.push_back(
          {dgm.edge(prop.crit_pred[v], v_old)});
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
      neighborhood.flip_candidates.push_back({});
      for (int j = 0; j < i; j++)
        neighborhood.flip_candidates.back().push_back(
            dgm.edge(critical_block[i], critical_block[j]));
    }
    for (int i = 1; i < critical_block.size() - 1; i++) {
      neighborhood.flip_candidates.push_back({});
      for (int j = i + 1; j < critical_block.size(); j++)
        neighborhood.flip_candidates.back().push_back(
            dgm.edge(critical_block[j], critical_block[i]));
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

        if (dgm.shuffle_graph[e].edge_type == disjunctive)
          critical_block = {v, u};
        else
          critical_block = {v_old, u};
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
  if (n > 0) {
    if (type != last) {
      neighborhood.flip_candidates.push_back(
          {dgm.edge(critical_block[1], critical_block[0])});
    }
    if (type != first && n > 2) {
      neighborhood.flip_candidates.push_back(
          {dgm.edge(critical_block[n - 1], critical_block[n - 2])});
    }
  }
}

} // namespace tsndgm
