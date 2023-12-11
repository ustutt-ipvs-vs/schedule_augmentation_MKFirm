#ifndef TSN_DGM_DIVERSIFICATION_H
#define TSN_DGM_DIVERSIFICATION_H

#include "neighborhood.h"
#include "selection.h"
#include "termination.h"
#include <random>

namespace tsndgm {

struct DiversificationConfig {
  size_t max_iterations;
};

template <class SelectionNeighborhood> class Diversification {
public:
  Diversification(DisjunctiveGraphModel &dgm, DiversificationConfig &config)
      : dgm(dgm), config(config), termination_criterion(config.max_iterations),
        selection_neighborhood(dgm) {}

  virtual void update_history(const NextSelection &next_selection) = 0;

  virtual NextSelection
  compute_next_selection(CriticalPath::Objective type) = 0;

  bool completed(size_t iteration, Delay objective) {
    return termination_criterion.satisfied(iteration, objective);
  };

  virtual ~Diversification() = default;

protected:
  DisjunctiveGraphModel &dgm;
  DiversificationConfig &config;

  DefaultTerminationCriterion termination_criterion;
  SelectionNeighborhood selection_neighborhood;
};

class DummyNeighborhood {
public:
  DummyNeighborhood(DisjunctiveGraphModel &dgm){};
};

class NoDiversification : public Diversification<DummyNeighborhood> {
public:
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  NoDiversification(DisjunctiveGraphModel &dgm, DiversificationConfig &config)
      : Diversification<DummyNeighborhood>(dgm, config) {}

  void update_history(const NextSelection &next_selection){};

  bool completed(size_t iteration, Delay objective) { return true; };

  NextSelection compute_next_selection(CriticalPath::Objective type) {
    return NextSelection();
  };
};

template <class SelectionNeighborhood>
class FrequencyCountDiversification
    : public Diversification<SelectionNeighborhood> {
public:
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;

  FrequencyCountDiversification(DisjunctiveGraphModel &dgm,
                                DiversificationConfig &config)
      : Diversification<SelectionNeighborhood>(dgm, config), gen(rd()) {}

  void update_history(const NextSelection &next_selection);

  NextSelection compute_next_selection(CriticalPath::Objective type);

protected:
  std::map<std::pair<V, V>, size_t> frequency_count;
  std::random_device rd;
  std::mt19937 gen;
};

// template <class SelectionNeighborhood, class TransformationHeuristic>
// class TransformationHeuristicDiversification
//     : public Diversification<SelectionNeighborhood> {
// public:
//   typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;
//   typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
//
//   TransformationHeuristicDiversification(DisjunctiveGraphModel &dgm,
//                                          DiversificationConfig &config)
//       : Diversification<SelectionNeighborhood>(dgm, config) {}
//
//   void update_history(const NextSelection &next_selection);
//
//   NextSelection compute_next_selection(CriticalPath::Objective type);
// };

} // namespace tsndgm

#include "diversification.tcc"

#endif // TSN_DGM_DIVERSIFICATION_H
