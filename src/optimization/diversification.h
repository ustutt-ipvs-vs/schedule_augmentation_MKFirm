#ifndef TSN_DGM_DIVERSIFICATION_H
#define TSN_DGM_DIVERSIFICATION_H

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

  virtual void run(CriticalPath::Objective type) = 0;

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

  void run(CriticalPath::Objective type){};
};

template <class SelectionNeighborhood>
class FrequencyCountDiversification
    : public Diversification<SelectionNeighborhood> {
public:
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  FrequencyCountDiversification(DisjunctiveGraphModel &dgm,
                                DiversificationConfig &config)
      : Diversification<SelectionNeighborhood>(dgm, config), gen(rd()) {}

  void update_history(const NextSelection &next_selection);

  void run(CriticalPath::Objective type);

protected:
  std::map<E, size_t> frequency_count;
  std::random_device rd;
  std::mt19937 gen;
};

} // namespace tsndgm

#include "diversification.tcc"

#endif // TSN_DGM_DIVERSIFICATION_H
