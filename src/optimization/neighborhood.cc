#include "neighborhood.h"
#include "../dgm/transmission_graph.h"

namespace tsndgm {

Neighborhood &SelectionFullNeighborhood::compute(CriticalPath::Result res) {
  typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

  TransmissionGraphProperty &prop =
      boost::get_property(dgm.transmission_graph, boost::graph_bundle);
  dgm.update_machine_successors();

  neighborhood.clear();
  V v = res.critical_vertex, v_old;

  while (v != prop.src) {
    E e = dgm.edge(prop.crit_pred[v], v);

    if (dgm.transmission_graph[e].edge_type == disjunctive) {
      neighborhood.flip_candidates.push_back({e});
    } else if (dgm.transmission_graph[e].edge_type == fifo) {
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
  if (critical_block.size() > 1) {
    for (int i = 1 + restriction; i < critical_block.size() - 1; i++) {
      // move critical_block[i] after critical_block[0]
      neighborhood.flip_candidates.push_back(
          {dgm.edge(critical_block[i], critical_block.front())});
    }
    for (int i = 0; i < critical_block.size() - 1 - restriction; i++) {
      // move critical_block[i] before critical_block[-1]
      neighborhood.flip_candidates.push_back(
          {dgm.edge(critical_block.back(), critical_block[i])});
    }
  }
}

Neighborhood &
SelectionCriticalBlockNeighborhood::compute(CriticalPath::Result res) {
  TransmissionGraphProperty &prop =
      boost::get_property(dgm.transmission_graph, boost::graph_bundle);
  dgm.update_machine_successors();
  neighborhood.clear();

  V v = res.critical_vertex, v_old;
  Edge edge;
  std::vector<V> critical_block;
  CriticalBlockType type = last;

  while (v != prop.src) {
    V u = prop.crit_pred[v];
    E e = dgm.edge(u, v);

    if (dgm.transmission_graph[e].edge_type != conjunctive) {
      if (edge == dgm.transmission_graph[u].edge) {
        critical_block.push_back(u);
      } else {
        critical_block_to_neighbors(critical_block, type);
        type = intermediate;

        if (dgm.transmission_graph[e].edge_type == disjunctive) {
          critical_block = {v, u};
        } else {
          E de = dgm.fifo_to_disjunctive_edge(e);
          critical_block = {target(de, dgm.transmission_graph), u};
        }

        edge = dgm.transmission_graph[u].edge;
      }
    }

    v_old = v;
    v = u;
  }
  critical_block_to_neighbors(critical_block, first);

  return neighborhood;
}

Neighborhood &CompressionNeighborhood::compute(CriticalPath::Result res) {
  typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

  dgm.update_machine_successors();
  neighborhood.clear();

  transmission_graph_t &transmission_graph = dgm.transmission_graph;
  TransmissionGraphProperty &prop =
      boost::get_property(dgm.transmission_graph, boost::graph_bundle);

  for (V v = res.critical_vertex; v != prop.src; v = prop.crit_pred[v]) {
    for (V u = prop.crit_pred[v]; u != prop.src; u = prop.crit_pred[u]) {
      auto [uv, found] = boost::edge(u, v, transmission_graph);
      if (found && transmission_graph[uv].edge_type != conjunctive &&
          !dgm.apriori_jitter_violation(uv))
        neighborhood.shuffle_candidates.push_back({uv});
    }
  }

  return neighborhood;
}

Neighborhood &
WirelessCompressionNeighborhood::compute(CriticalPath::Result res) {
  typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

  dgm.update_machine_successors();
  neighborhood.clear();

  transmission_graph_t &transmission_graph = dgm.transmission_graph;
  TransmissionGraphProperty &prop =
      boost::get_property(dgm.transmission_graph, boost::graph_bundle);

  for (V v = res.critical_vertex; v != prop.src; v = prop.crit_pred[v]) {
    if (v == prop.sink ||
        dgm.network->get_data_link_property(transmission_graph[v].edge).type ==
            wired)
      continue;
    for (V u = prop.crit_pred[v]; u != prop.src; u = prop.crit_pred[u]) {
      auto [uv, found] = boost::edge(u, v, transmission_graph);
      if (found && transmission_graph[uv].edge_type != conjunctive &&
          !dgm.apriori_jitter_violation(uv))
        neighborhood.shuffle_candidates.push_back({uv});
    }
  }

  return neighborhood;
}

} // namespace tsndgm
