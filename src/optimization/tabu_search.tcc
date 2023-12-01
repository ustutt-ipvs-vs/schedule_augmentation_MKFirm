#ifndef TSN_DGM_TABUSEARCH_TCC
#define TSN_DGM_TABUSEARCH_TCC

#include "tabu_search.h"

namespace tsndgm {

template <class TerminationCriterion, class Intensification,
          class Diversification>
void TabuSearch::run(Config &config) {
  BestSelection best_selection(config.commit_index);
  TerminationCriterion termination_criterion(config.global_iterations);
  size_t phase;

  for (phase = 0;
       !termination_criterion.satisfied(phase, best_selection.objective);
       phase++) {
    std::cout << "Phase " << phase << ":" << std::endl;

    Intensification int_phase(dgm, config.int_config);
    Diversification div_phase(dgm, config.div_config);

    run_intensification_phase<Intensification, Diversification>(
        config, int_phase, div_phase, best_selection);
    run_diversification_phase<Diversification>(config, div_phase);
  }

  std::cout << "Global Solution: " << best_selection.objective << std::endl;
}

template <class Intensification, class Diversification>
void TabuSearch::run_intensification_phase(Config &config,
                                           Intensification &int_phase,
                                           Diversification &div_phase,
                                           BestSelection &best_selection) {
  size_t iteration;
  NextSelection next_selection;

  for (iteration = 0; !int_phase.completed(iteration, next_selection.objective);
       iteration++) {
    next_selection = int_phase.compute_next_selection(config.type, true);
    div_phase.update_history(next_selection);

    if (next_selection.operation == flip)
      dgm.complete_flip(next_selection.e);
    else
      dgm.lazy_shuffle(next_selection.e);

    if (next_selection.objective < best_selection.objective) {
      dgm.commit_all(best_selection.commit_index);
      best_selection.objective = next_selection.objective;
    }
  }
  std::cout << " -> intensify: " << int_phase.best_selection.objective
            << " after " << iteration << " iterations" << std::endl;

  dgm.restore_commit(best_selection.commit_index, true);
}

template <class Diversification>
void TabuSearch::run_diversification_phase(Config &config,
                                           Diversification &phase) {
  phase.run(config.type);
}

} // namespace tsndgm

#endif // TSN_DGM_TABUSEARCH_TCC
