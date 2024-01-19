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
  int extension_level = 0;
};

class SelectionNeighborhood {
public:
  SelectionNeighborhood(DisjunctiveGraphModel &dgm) : dgm(dgm) {}

  virtual const Neighborhood &compute(CriticalPath::Result res) = 0;
  virtual const Neighborhood &extend(CriticalPath::Result res,
                                     int extension_level) = 0;

  virtual ~SelectionNeighborhood() = default;

  static const int max_extension = 0;

protected:
  DisjunctiveGraphModel &dgm;
};

class SelectionFullNeighborhood : public SelectionNeighborhood {
public:
  SelectionFullNeighborhood(DisjunctiveGraphModel &dgm)
      : SelectionNeighborhood(dgm) {}

  const Neighborhood &compute(CriticalPath::Result res);
  const Neighborhood &extend(CriticalPath::Result res, int extension_level) {
    if (extension_level == 0)
      return compute(res);
    throw std::runtime_error("no extension available");
  };

  static const int max_extension = 0;

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
  const Neighborhood &extend(CriticalPath::Result res, int extension_level) {
    if (extension_level == 0)
      return compute(res);
    throw std::runtime_error("no extension available");
  };

  static const int max_extension = 0;

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
      : SelectionCriticalBlockNeighborhood(dgm), extended_neighborhood(dgm) {}
  const Neighborhood &extend(CriticalPath::Result res, int extension_level) {
    if (extension_level == 0)
      return compute(res);
    else if (extension_level == 1)
      return extended_neighborhood.compute(res);
    throw std::runtime_error("extension not available");
  };

  static const int max_extension = 1;

protected:
  SelectionCriticalBlockNeighborhood extended_neighborhood;

  void critical_block_to_neighbors(const std::vector<V> &critical_block,
                                   CriticalBlockType type);
};

class CompressionNeighborhood : public SelectionNeighborhood {
public:
  CompressionNeighborhood(DisjunctiveGraphModel &dgm)
      : SelectionNeighborhood(dgm) {}

  const Neighborhood &compute(CriticalPath::Result res);
  const Neighborhood &extend(CriticalPath::Result res, int extension_level) {
    if (extension_level == 0)
      return compute(res);
    throw std::runtime_error("no extension available");
  };

  static const int max_extension = 0;

private:
  Neighborhood neighborhood;
};

} // namespace tsndgm

#endif // TSN_DGM_NEIGHBORHOOD_H
