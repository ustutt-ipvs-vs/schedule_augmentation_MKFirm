#ifndef TSN_DGM_TABUSEARCH_H
#define TSN_DGM_TABUSEARCH_H

#include "../dgm/dgm.h"
#include "../heuristics/transformation.h"
#include "communicator.h"
#include "intensification.h"
#include "relinking_phase.h"
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
    RelinkingConfig relinking_config;
    size_t diversification_rounds;
    size_t initial_solutions = 1;
    bool compress = false;
    Delay termination_bound = 0;
    size_t commit_index = 2;
  };

  TabuSearch(const std::shared_ptr<NetworkTopology> &network,
             const std::vector<MessageStream> &streams)
      : dgm(network, streams) {
    relinking.initialize(&this->dgm);
    start = std::chrono::high_resolution_clock::now();
  }

  TabuSearch(DisjunctiveGraphModel &dgm) : dgm(dgm) {
    relinking.initialize(&this->dgm);
    start = std::chrono::high_resolution_clock::now();
  }

  template <class InitialHeuristic, class TerminationCriterion,
            class Intensification, class ExhaustiveSearch,
            class TransformationHeuristic>
  void run(Config &config);

  template <class InitialHeuristic, class Intensification>
  BestSelection
  run_initial_phase(int initial_solutions, IntensificationConfig &config,
                    CriticalPath::Objective type, Delay termination_bound = 0);

  template <class Intensification>
  BestSelection run_intensification_phase(IntensificationConfig &config,
                                          CriticalPath::Objective type,
                                          Delay termination_bound = 0);

  template <class Intensification>
  BestSelection run_exhaustive_search(BestSelection &best_selection,
                                      BestSelection &int_phase,
                                      ExhaustiveSearchConfig &config,
                                      CriticalPath::Objective type,
                                      Delay termination_bound = 0);

  template <class Intensification>
  BestSelection run_relinking_phase(RelinkingConfig &config,
                                    CriticalPath::Objective type,
                                    Delay termination_bound);

  template <class Intensification>
  BestSelection run_compression_phase(IntensificationConfig &config,
                                      CriticalPath::Objective type,
                                      Delay termination_bound);

  DisjunctiveGraphModel dgm;
  Relinking relinking;
  BestSelection best_selection;
  Communicator com;

private:
  auto update_best_selection(BestSelection &best_selection,
                             NextSelection &res) {
    best_selection.objective = res.objective;
    best_selection.committed = false;
  }

  auto update_best_selection(BestSelection &best_selection,
                             BestSelection &res) {
    best_selection.objective = res.objective;
    std::swap(*best_selection.commit_index, *res.commit_index);
    best_selection.committed = true;
  }

  std::chrono::high_resolution_clock::time_point start;
};

} // namespace tsndgm

#include "tabu_search.tcc"

#endif // TSN_DGM_TABUSEARCH_H
