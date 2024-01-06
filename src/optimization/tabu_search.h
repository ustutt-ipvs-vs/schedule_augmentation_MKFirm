#ifndef TSN_DGM_TABUSEARCH_H
#define TSN_DGM_TABUSEARCH_H

#include "../dgm/dgm.h"
#include "../heuristics/transformation.h"
#include "communicator.h"
#include "intensification.h"
#include "selection.h"
#include <chrono>

namespace tsndgm {

class TabuSearch {
public:
  struct Config {
    size_t maxit;
    CriticalPath::Objective type;
    IntensificationConfig int_config;
    ExhaustiveSearchConfig exhaustive_search_config;
    size_t diversification_rounds;
    size_t commit_index = 2;
  };

  TabuSearch(const std::shared_ptr<NetworkTopology> &network,
             const std::vector<MessageStream> &streams)
      : dgm(network, streams) {}

  TabuSearch(DisjunctiveGraphModel &dgm) : dgm(dgm) {}

  template <class InitialHeuristic, class TerminationCriterion,
            class Intensification, class ExhaustiveSearch,
            class TransformationHeuristic>
  void run(Config &config);

  DisjunctiveGraphModel dgm;
  BestSelection best_selection;
  Communicator com;

private:
  template <class Intensification>
  BestSelection run_intensification_phase(IntensificationConfig &config,
                                          CriticalPath::Objective type,
                                          bool restore_local_minimum = true);

  template <class Intensification>
  std::pair<BestSelection, bool>
  run_exhaustive_search(BestSelection &best_selection, BestSelection &int_phase,
                        ExhaustiveSearchConfig &config,
                        CriticalPath::Objective type);

  template <class S>
  auto update_best_selection(BestSelection &best_selection, S &res) {
    best_selection.objective = res.objective;
    best_selection.secondary_objective = res.secondary_objective;
    best_selection.committed = false;
  }

  std::chrono::high_resolution_clock::time_point start;
};

} // namespace tsndgm

#include "tabu_search.tcc"

#endif // TSN_DGM_TABUSEARCH_H
