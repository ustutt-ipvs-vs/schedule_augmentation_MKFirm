#ifndef TSN_DGM_TABUSEARCH_H
#define TSN_DGM_TABUSEARCH_H

#include "../dgm/dgm.h"
#include "diversification.h"
#include "intensification.h"
#include "selection.h"

namespace tsndgm {

class TabuSearch {
public:
  struct Config {
    size_t global_iterations;
    CriticalPath::Objective type;
    IntensificationConfig int_config;
    DiversificationConfig div_config;
    size_t commit_index; //!< for best global selection
  };

  TabuSearch(const std::shared_ptr<NetworkTopology> &network,
             const std::vector<MessageStream> &streams)
      : dgm(network, streams) {}

  TabuSearch(DisjunctiveGraphModel &dgm) : dgm(dgm) {}
  TabuSearch(DisjunctiveGraphModel &&dgm) : dgm(std::move(dgm)) {}

  template <class TerminationCriterion, class Intensification,
            class Diversification>
  void run(Config &config);

  DisjunctiveGraphModel dgm;

private:
  template <class Intensification, class Diversification>
  void run_intensification_phase(Config &config, Intensification &int_phase,
                                 Diversification &div_phase,
                                 BestSelection &best_selection);
  template <class Diversification>
  void run_diversification_phase(Config &config, Diversification &phase);
};

} // namespace tsndgm

#include "tabu_search.tcc"

#endif // TSN_DGM_TABUSEARCH_H
