#ifndef TSN_DGM_TABUSEARCH_TCC
#define TSN_DGM_TABUSEARCH_TCC

#include "tabu_search.h"

namespace tsndgm {

template <class InitialHeuristic, class TerminationCriterion,
          class Intensification, class ExhaustiveSearch,
          class TransformationHeuristic>
void TabuSearch::run(Config &config) {
  best_selection = BestSelection(&config.commit_index);
  BestSelection res;
  size_t phase;

  auto print_result = [&](Delay res) {
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::seconds>(stop - start);
    std::cout << "  -> Result: " << res << " (" << duration << ") "
              << dgm.total_flips << std::endl;
  };
  auto update_best_selection = [&](BestSelection &res) {
    this->update_best_selection(best_selection, res);
    relinking.update_candidates(best_selection);
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::seconds>(stop - start);
    std::cout << "  -> New Best Selection: " << best_selection.objective << " ("
              << duration << ")" << std::endl;
  };

  std::cout << "Phase 0:\n Initial: " << std::endl;
  best_selection = run_initial_phase<InitialHeuristic, Intensification>(
      config.initial_solutions, config.int_config, config.type,
      config.termination_bound);
  best_selection = com.exchange_best_selection(dgm, best_selection);
  print_result(best_selection.objective);

  TransformationHeuristic heuristic(dgm, config.type);
  TerminationCriterion termination_criterion(config.maxit,
                                             config.termination_bound);
  for (phase = 0;
       !termination_criterion.satisfied(phase, best_selection.objective);
       phase++) {
    std::cout << "Phase " << phase + 1 << ":" << std::endl;
    std::cout << " Diversify ";
    if (relinking.ready(config.relinking_config)) {
      std::cout << "(Relinking):" << std::endl;
      res = run_relinking_phase<Intensification>(
          config.relinking_config, config.type, best_selection.objective);
    } else {
      std::cout << "(Transformation):" << std::endl;
      heuristic.transform(config.diversification_rounds);
      res = run_intensification_phase<Intensification>(
          config.int_config, config.type, config.termination_bound);
      com.sync();
    }
    relinking.update_candidates(res);
    res = com.exchange_best_selection(dgm, res);
    relinking.update_candidates(res);
    print_result(res.objective);

    std::cout << " Intensify: " << std::endl;
    res = run_exhaustive_search<ExhaustiveSearch>(
        best_selection, res, config.exhaustive_search_config, config.type);
    res = com.exchange_best_selection(dgm, res);
    relinking.update_candidates(res);
    if (res < best_selection)
      update_best_selection(res);
    print_result(res.objective);
  }
  std::cout << "Global Solution: " << best_selection.objective << std::endl;
  dgm.restore_commit(*best_selection.commit_index);
  std::cout << dgm.critical_path(config.type).objective << std::endl;

  if (config.compress) {
    run_compression_phase<Intensification>(config.int_config, config.type,
                                           config.termination_bound);
    best_selection = com.exchange_best_selection(dgm, best_selection);
    std::cout << "Compressed Solution: " << best_selection.objective
              << std::endl;
  }
}

template <class InitialHeuristic, class Intensification>
BestSelection TabuSearch::run_initial_phase(int initial_solutions,
                                            IntensificationConfig &config,
                                            CriticalPath::Objective type,
                                            Delay termination_bound) {
  InitialHeuristic initial_heuristic(dgm, type);
  for (int i = 0; i < initial_solutions; i++) {
    initial_heuristic.generate();
    BestSelection res = run_intensification_phase<Intensification>(
        config, type, termination_bound);
    relinking.update_candidates(res);
    if (res < best_selection) {
      update_best_selection(this->best_selection, res);
    }
    Communicator::State state = res.objective <= termination_bound
                                    ? Communicator::found_better
                                    : Communicator::running;
    state = com.exchange_state(state);
    if (state != Communicator::running)
      return best_selection;
  }
  com.sync();

  return best_selection;
}

template <class Intensification>
BestSelection
TabuSearch::run_intensification_phase(IntensificationConfig &config,
                                      CriticalPath::Objective type,
                                      Delay termination_bound) {
  Intensification int_phase(dgm, config, termination_bound);
  NextSelection next_selection;
  BestSelection best_selection(&config.commit_index,
                               dgm.critical_path(type).objective);
  dgm.commit_all(config.commit_index);
  best_selection.committed = true;

  size_t iteration;
  for (iteration = 0; !int_phase.completed(iteration, next_selection.objective);
       iteration++) {
    next_selection = int_phase.compute_next_selection(type);

    if (next_selection < best_selection) {
      update_best_selection(best_selection, next_selection);
    } else if (!best_selection.committed) {
      // commit best known solution if we start with uphill moves
      dgm.critical_path(type);
      dgm.commit_all(*best_selection.commit_index);
      best_selection.committed = true;
    }

    if (next_selection.operation == flip) {
      try {
        dgm.complete_flip(next_selection.edges);
      } catch (FlipGraphException &e) {
        if (config.recursive_shuffle) {
          dgm.complete_shuffle(e.required_shuffle, false);
          int_phase.clear_tabu_list();
        } else {
          continue;
        }
      }
    } else {
      dgm.complete_shuffle(next_selection.edges, false);
      int_phase.clear_tabu_list();
    }
  }

  // best_selection might not be committed if next_selection.objective ==
  // best_selection.objective for maxit iterations
  if (!best_selection.committed) {
    dgm.commit_all(*best_selection.commit_index);
    best_selection.committed = true;
  }

  return best_selection;
}

template <class Intensification>
BestSelection TabuSearch::run_exhaustive_search(BestSelection &best_selection,
                                                BestSelection &int_phase,
                                                ExhaustiveSearchConfig &config,
                                                CriticalPath::Objective type,
                                                Delay termination_bound) {
  BestSelection res;
  SelectionCriticalBlockNeighborhood selection_neighborhood(dgm);
  auto neighborhood =
      selection_neighborhood.compute(this->dgm.critical_path(type))
          .flip_candidates;
  auto partition = com.partition(neighborhood.begin(), neighborhood.end());

  for (auto it = partition.first; it != partition.second; ++it) {
    auto edges = *it;

    // edge descriptors in neighborhood might be invalidated after restoring
    for (auto &e : edges)
      e = dgm.edge(e);

    this->dgm.complete_flip(edges);
    res = run_intensification_phase<Intensification>(config, type,
                                                     termination_bound);
    Communicator::State state = res < best_selection
                                    ? Communicator::found_better
                                    : Communicator::running;
    state = com.exchange_state(state);
    if (state != Communicator::running)
      return res;
    dgm.restore_commit(*int_phase.commit_index);
  }
  com.sync();

  return res;
}

template <class Intensification>
BestSelection TabuSearch::run_relinking_phase(RelinkingConfig &config,
                                              CriticalPath::Objective type,
                                              Delay bound) {
  BestSelection best_selection(&config.best_commit_index);

  while (!best_selection.committed) {
    auto [initial_index, guiding_index] = relinking.sample(config);
    auto differing_machines =
        relinking.compute_differing_machines(initial_index, guiding_index);
    dgm.decode(relinking.encoded_best_selections[initial_index].buf);
    EncodedSelection &guiding_selection =
        relinking.encoded_best_selections[guiding_index];
    int steps = config.min_separation;

    for (int it = 0;; it++) {
      bool success =
          relinking.step_towards(guiding_selection, differing_machines, steps);
      if (!success)
        break;
      dgm.commit_all(config.backup_commit_index);

      BestSelection res = run_intensification_phase<Intensification>(
          config.local_search_config, type);
      Communicator::State state = res < this->best_selection
                                      ? Communicator::found_better
                                      : Communicator::running;
      state = com.exchange_state(state);
      if (state == Communicator::found_better) {
        return res;
      } else if (state == Communicator::terminated) {
        return best_selection;
      } else if (res <= best_selection &&
                 relinking.differs(res, initial_index) &&
                 relinking.differs(res, guiding_index)) {
        steps = config.min_separation;
        update_best_selection(best_selection, res);
      } else {
        steps *= 2;
      }

      dgm.restore_commit(config.backup_commit_index);
    }
  }

  com.sync();
  return best_selection;
}

template <class Intensification>
BestSelection TabuSearch::run_compression_phase(IntensificationConfig &config,
                                                CriticalPath::Objective type,
                                                Delay termination_bound) {
  config.recursive_shuffle = true;

  BestSelection res;
  CompressionNeighborhood neighborhood(dgm);
  bool improvement_found = true;
  while (improvement_found) {
    improvement_found = false;
    dgm.restore_commit(*best_selection.commit_index);

    auto &shuffle_candidates =
        neighborhood.compute(dgm.critical_path(type)).shuffle_candidates;

    for (auto edges : shuffle_candidates) {
      // edge descriptors in neighborhood might be invalidated after restoring
      for (auto &e : edges)
        e = dgm.edge(e);

      try {
        dgm.complete_shuffle(edges);
        res = run_intensification_phase<Intensification>(config, type,
                                                         termination_bound);
      } catch (UnfixableCycleException &e) {
        // complete_shuffle already restores the fallback, there is no need
        // to call undo_last_shuffle
        continue;
      }

      if (res < best_selection) {
        dgm.restore_commit(*res.commit_index);
        update_best_selection(best_selection, res);
        improvement_found = true;
        auto stop = std::chrono::high_resolution_clock::now();
        auto duration = duration_cast<std::chrono::seconds>(stop - start);
        std::cout << "  -> New Compressed Selection: "
                  << best_selection.objective << " (" << duration << ")"
                  << std::endl;
      }

      dgm.undo_last_shuffle();
    }
  }

  config.recursive_shuffle = false;
  return best_selection;
}

} // namespace tsndgm

#endif // TSN_DGM_TABUSEARCH_TCC
