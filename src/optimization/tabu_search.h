#ifndef TSN_DGM_TABUSEARCH_H
#define TSN_DGM_TABUSEARCH_H

#include "../dgm/dgm.h"
#include "../heuristics/transform.h"
#include "communicator.h"
#include "diversification.h"
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
    DiversificationConfig div_config;
    size_t commit_index;
  };

  TabuSearch(const std::shared_ptr<NetworkTopology> &network,
             const std::vector<MessageStream> &streams)
      : dgm(network, streams) {}

  TabuSearch(DisjunctiveGraphModel &dgm) : dgm(dgm) {}

  template <class TerminationCriterion, class Intensification,
            class Diversification, class TransformationHeuristic>
  void run(Config &config);

  DisjunctiveGraphModel dgm;
  BestSelection best_selection;
  Communicator com;

private:
  template <class Intensification, class Diversification>
  void run_intensification_phase(Config &config, Intensification &int_phase,
                                 Diversification &div_phase,
                                 BestSelection &best_selection);

  template <class Intensification, class Diversification>
  void run_diversification_phase(Config &config, Intensification &int_phase,
                                 Diversification &div_phase,
                                 BestSelection &best_selection);

  template <class Intensification, class Diversification>
  bool run_exhaustive_search(Config &config, Intensification &int_phase,
                             Diversification &div_phase,
                             BestSelection &best_selection);

  std::chrono::high_resolution_clock::time_point start;
};

} // namespace tsndgm

#include "tabu_search.tcc"

#endif // TSN_DGM_TABUSEARCH_H
