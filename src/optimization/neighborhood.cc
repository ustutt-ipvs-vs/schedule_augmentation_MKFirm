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
  V v = res.critical_vertex;

  while (v != prop.src) {
    E e = boost::edge(prop.crit_pred[v], v, dgm.shuffle_graph).first;

    // only by flipping disjunctive and FIFO edges, or merging FIFO edges
    // we can potentially reduce the objective
    if (dgm.shuffle_graph[e].edge_type == disjunctive) {
      neighborhood.flip_candidates.push_front(e);
    } else if (dgm.shuffle_graph[e].edge_type == fifo) {
      neighborhood.flip_candidates.push_front(e);
      neighborhood.shuffle_candidates.push_front(e);
    }

    v = prop.crit_pred[v];
  }

  return neighborhood;
}

} // namespace tsndgm
