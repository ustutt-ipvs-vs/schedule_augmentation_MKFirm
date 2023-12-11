#ifndef TSN_DGM_HEURISTIC_TRANSFORM_H
#define TSN_DGM_HEURISTIC_TRANSFORM_H

#include "../dgm/dgm.h"
#include "../optimization/neighborhood.h"
#include "../optimization/selection.h"
#include <random>

namespace tsndgm {

class TransformSelectionHeuristic {
public:
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  TransformSelectionHeuristic(DisjunctiveGraphModel &dgm) : dgm(dgm) {}

  virtual void transform(CriticalPath::Result res) = 0;

protected:
  DisjunctiveGraphModel &dgm;
};

class NoTransformation : public TransformSelectionHeuristic {
public:
  NoTransformation(DisjunctiveGraphModel &dgm)
      : TransformSelectionHeuristic(dgm) {}

  void transform(CriticalPath::Result res){};
};

class CriticalBlockTransformSelectionHeuristic
    : public TransformSelectionHeuristic {
public:
  class CriticalMachines : public SelectionCriticalBlockNeighborhood {
  public:
    CriticalMachines(DisjunctiveGraphModel &dgm)
        : SelectionCriticalBlockNeighborhood(dgm) {}

    std::vector<Edge> edges;
    std::vector<size_t> weights;

  protected:
    void critical_block_to_neighbors(const std::vector<V> &critical_block,
                                     CriticalBlockType type) {
      if (critical_block.size() > 0) {
        edges.push_back(dgm.shuffle_graph[critical_block[0]].edge);
        weights.push_back(critical_block.size());
      }
    }
  };

  CriticalBlockTransformSelectionHeuristic(DisjunctiveGraphModel &dgm)
      : TransformSelectionHeuristic(dgm), gen(rd()) {}

  void transform(CriticalPath::Result res);

protected:
  std::random_device rd;
  std::mt19937 gen;
};

} // namespace tsndgm
#endif // TSN_DGM_HEURISTIC_TRANSFORM_H
