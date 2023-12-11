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

  std::vector<std::list<E>> flip_candidates;
  std::vector<std::list<E>> shuffle_candidates;
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

class SelectionCriticalBlockNeighborhood : public SelectionNeighborhood {
public:
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  SelectionCriticalBlockNeighborhood(DisjunctiveGraphModel &dgm)
      : SelectionNeighborhood(dgm) {}

  const Neighborhood &compute(CriticalPath::Result res);

protected:
  Neighborhood neighborhood;

  enum CriticalBlockType { first, intermediate, last };
  virtual void critical_block_to_neighbors(const std::vector<V> &critical_block,
                                           CriticalBlockType type);
};

class ReducedSelectionCriticalBlockNeighborhood
    : public SelectionCriticalBlockNeighborhood {
public:
  ReducedSelectionCriticalBlockNeighborhood(DisjunctiveGraphModel &dgm)
      : SelectionCriticalBlockNeighborhood(dgm) {}

protected:
  void critical_block_to_neighbors(const std::vector<V> &critical_block,
                                   CriticalBlockType type);
};

} // namespace tsndgm

#endif // TSN_DGM_NEIGHBORHOOD_H
