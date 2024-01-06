#ifndef TSN_DGM_TABUSEARCH_TCC
#define TSN_DGM_TABUSEARCH_TCC

#include "tabu_search.h"

namespace tsndgm {

template <class InitialHeuristic, class TerminationCriterion,
          class Intensification, class ExhaustiveSearch,
          class TransformationHeuristic>
void TabuSearch::run(Config &config) {
  best_selection = BestSelection(config.commit_index);
  size_t phase;

  start = std::chrono::high_resolution_clock::now();
  auto print_result = [&]() {
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::seconds>(stop - start);
    std::cout << " -> Result: " << dgm.critical_path(config.type).objective
              << " (" << duration << ")" << std::endl;
  };

  std::cout << "Phase 0:\n Initial: " << std::endl;
  InitialHeuristic initial_heuristic(dgm, config.type);
  initial_heuristic.generate();
  print_result();

  TransformationHeuristic heuristic(dgm, config.type);

  TerminationCriterion termination_criterion(config.maxit);
  for (phase = 0; !termination_criterion.satisfied(
           phase, best_selection.objective, best_selection.secondary_objective);
       phase++) {
    std::cout << " Intensify:" << std::endl;
    BestSelection res = run_intensification_phase<Intensification>(
        config.int_config, config.type, false);
    bool continue_exhaustive_search = true;
    while (continue_exhaustive_search) {
      res = com.exchange_best_selection(dgm, res);
      if (res < best_selection) {
        update_best_selection(best_selection, res);
        auto stop = std::chrono::high_resolution_clock::now();
        auto duration = duration_cast<std::chrono::seconds>(stop - start);
        std::cout << " -> New Best Selection: " << best_selection.objective
                  << " " << best_selection.secondary_objective << " ("
                  << duration << ")" << std::endl;
      }

      boost::tie(res, continue_exhaustive_search) =
          run_exhaustive_search<ExhaustiveSearch>(
              best_selection, res, config.exhaustive_search_config,
              config.type);
      std::swap(config.int_config.commit_index,
                config.exhaustive_search_config.commit_index);
    }
    print_result();

    if (!best_selection.committed) {
      std::swap(config.exhaustive_search_config.commit_index,
                best_selection.commit_index);
      best_selection.committed = true;
    } else {
      dgm.restore_commit(best_selection.commit_index, false);
    }

    if (!termination_criterion.satisfied(phase + 1, best_selection.objective)) {
      std::cout << "Phase " << phase + 1 << ":" << std::endl;
      std::cout << " Diversify:" << std::endl;
      heuristic.transform(config.diversification_rounds);
      print_result();
    }
  }

  std::cout << "Global Solution: " << best_selection.objective << std::endl;
}

template <class Intensification>
BestSelection
TabuSearch::run_intensification_phase(IntensificationConfig &config,
                                      CriticalPath::Objective type,
                                      bool restore_local_minimum) {
  Intensification int_phase(dgm, config);
  NextSelection next_selection, prev_selection;
  BestSelection best_selection(config.commit_index);

  size_t iteration;
  for (iteration = 0; !int_phase.completed(iteration, next_selection.objective);
       iteration++) {
    // We consider two objectives:
    // Primary objective: selection's objective (e.g. makespan, tardiness)
    // Secondary objective: average objective of selection's neighborhood
    next_selection = int_phase.compute_next_selection(type);
    prev_selection.secondary_objective = next_selection.secondary_objective;

    if (prev_selection < best_selection) {
      update_best_selection(best_selection, prev_selection);
    }
    if (!best_selection.committed &&
        next_selection.objective > best_selection.objective) {
      // commit best known solution if we start with uphill moves
      dgm.commit_all(best_selection.commit_index);
      best_selection.committed = true;
    }

    if (next_selection.operation == flip)
      dgm.complete_flip(next_selection.edges);
    else
      dgm.complete_shuffle(next_selection.edges);

    prev_selection = next_selection;
  }

  // best_selection might not be committed if next_selection.objective ==
  // best_selection.objective for maxit iterations
  if (!best_selection.committed) {
    dgm.commit_all(best_selection.commit_index);
    best_selection.committed = true;
  } else if (restore_local_minimum) {
    dgm.restore_commit(best_selection.commit_index);
  }

  return best_selection;
}

template <class Intensification>
std::pair<BestSelection, bool> TabuSearch::run_exhaustive_search(
    BestSelection &best_selection, BestSelection &int_phase,
    ExhaustiveSearchConfig &config, CriticalPath::Objective type) {
  CriticalPath::Result res = this->dgm.critical_path(type);
  typename Intensification::ISelectionNeighborhood selection_neighborhood(dgm);
  auto neighborhood = selection_neighborhood.compute(res).flip_candidates;
  auto partition = com.partition(neighborhood.begin(), neighborhood.end());

  for (auto it = partition.first; it < partition.second; ++it) {
    auto edges = *it;

    // edge descriptors in neighborhood might be invalidated after restoring
    for (auto &e : edges)
      e = dgm.edge(e);

    this->dgm.complete_flip(edges);
    BestSelection res =
        run_intensification_phase<Intensification>(config, type, false);
    if (com.exchange_state(res < best_selection)) {
      return {res, true};
    }
    dgm.restore_commit(int_phase.commit_index);
  }
  if (com.smaller_partition(neighborhood.begin(), neighborhood.end())) {
    return {int_phase, com.exchange_state(false)};
  } else {
    return {int_phase, false};
  }
}

} // namespace tsndgm

#endif // TSN_DGM_TABUSEARCH_TCC
