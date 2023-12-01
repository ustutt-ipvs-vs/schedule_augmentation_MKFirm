#ifndef TSN_DGM_NEIGHBORHOOD_H
#define TSN_DGM_NEIGHBORHOOD_H

#include "../dgm/dgm.h"

namespace tsndgm {

struct Neighborhood {
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  void clear() {
    flip_candidates.clear();
    shuffle_candidates.clear();
  }

  std::list<E> flip_candidates;
  std::list<E> shuffle_candidates;
};

class SelectionNeighborhood {
public:
  SelectionNeighborhood(DisjunctiveGraphModel &dgm) : dgm(dgm) {}

  virtual const Neighborhood &compute(CriticalPath::Result res) = 0;

protected:
  DisjunctiveGraphModel &dgm;
};

class SelectionFullNeighborhood : public SelectionNeighborhood {
public:
  SelectionFullNeighborhood(DisjunctiveGraphModel &dgm)
      : SelectionNeighborhood(dgm) {}

  const Neighborhood &compute(CriticalPath::Result res);

private:
  Neighborhood neighborhood;
};

} // namespace tsndgm

#endif // TSN_DGM_NEIGHBORHOOD_H
