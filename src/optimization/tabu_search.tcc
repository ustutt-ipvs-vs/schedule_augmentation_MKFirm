#ifndef TSN_DGM_TABUSEARCH_TCC
#define TSN_DGM_TABUSEARCH_TCC

#include "tabu_search.h"

namespace tsndgm {

template <class TerminationCriterion, class Intensification,
          class Diversification, class TransformationHeuristic>
void TabuSearch::run(Config &config) {
  best_selection = BestSelection(config.commit_index);
  size_t phase;

  start = std::chrono::high_resolution_clock::now();
  Diversification div_phase(dgm, config.div_config);
  Intensification int_phase(dgm, config.int_config);
  TransformationHeuristic heuristic(dgm);

  for (phase = 0;
       !termination_criterion.satisfied(phase, best_selection.objective);
       phase++) {
    std::cout << "Phase " << phase << ":" << std::endl;

    run_intensification_phase<Intensification, Diversification>(
        config, int_phase, div_phase, best_selection);

    if (!termination_criterion.satisfied(phase, best_selection.objective)) {
      run_diversification_phase<Intensification, Diversification>(
          config, int_phase, div_phase);
      heuristic.transform(dgm.critical_path(config.type));
    }
  }

  std::cout << "Global Solution: " << best_selection.objective << std::endl;
}

template <class Intensification, class Diversification>
void TabuSearch::run_intensification_phase(Config &config,
                                           Intensification &int_phase,
                                           Diversification &div_phase,
                                           BestSelection &best_selection) {
  size_t iteration;
  NextSelection next_selection, prev_selection;

  auto update_best_selection = [&](NextSelection &next_selection) {
    best_selection.objective = next_selection.objective;
    best_selection.secondary_objective = next_selection.secondary_objective;
    best_selection.committed = false;
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::seconds>(stop - start);
    std::cout << "  -> New Best Solution " << best_selection.objective << " "
              << best_selection.secondary_objective << " (" << duration << ")"
              << std::endl;
  };

  int_phase.reset_phase();

  for (iteration = 0; !int_phase.completed(iteration, next_selection.objective);
       iteration++) {
    next_selection = int_phase.compute_next_selection(config.type);
    prev_selection.secondary_objective = next_selection.secondary_objective;

    if (prev_selection.objective == best_selection.objective &&
        prev_selection.secondary_objective < best_selection.secondary_objective)
      update_best_selection(prev_selection);

    // commit best known solution if we start with uphill moves
    if (next_selection.objective > best_selection.objective &&
        !best_selection.committed) {
      dgm.commit_all(best_selection.commit_index);
      best_selection.committed = true;
    }

    if (next_selection.operation == flip)
      dgm.complete_flip(next_selection.edges);
    else
      dgm.lazy_shuffle(next_selection.edges);

    div_phase.update_history(next_selection);

    if (next_selection.objective < best_selection.objective)
      update_best_selection(next_selection);

    prev_selection = next_selection;
  }

  auto stop = std::chrono::high_resolution_clock::now();
  auto duration = duration_cast<std::chrono::seconds>(stop - start);
  std::cout << " -> Result: " << int_phase.best_selection.objective << " after "
            << iteration << " iterations (" << duration << ")" << std::endl;

  if (best_selection.objective <= next_selection.objective)
    dgm.restore_commit(best_selection.commit_index, false);
}

template <class Intensification, class Diversification>
void TabuSearch::run_diversification_phase(Config &config,
                                           Intensification &int_phase,
                                           Diversification &div_phase) {
  size_t iteration;
  NextSelection next_selection;

  std::cout << " Diversify:" << std::endl;

  for (iteration = 0; !div_phase.completed(iteration, next_selection.objective);
       iteration++) {
    next_selection = div_phase.compute_next_selection(config.type);

    if (next_selection.operation == flip)
      dgm.complete_flip(next_selection.edges);
    else
      dgm.lazy_shuffle(next_selection.edges);

    int_phase.update_tabu_list(
        {next_selection.edges, next_selection.objective});
  }

  auto stop = std::chrono::high_resolution_clock::now();
  auto duration = duration_cast<std::chrono::seconds>(stop - start);
  std::cout << " Result: " << next_selection.objective << " after " << iteration
            << " iterations (" << duration << ")" << std::endl;
}

} // namespace tsndgm

#endif // TSN_DGM_TABUSEARCH_TCC
