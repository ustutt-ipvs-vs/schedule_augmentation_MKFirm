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
  auto update_best_selection = [&](BestSelection &res, bool swap = true) {
    if (res < best_selection) {
      this->update_best_selection(best_selection, res, swap);
      auto stop = std::chrono::high_resolution_clock::now();
      auto duration = duration_cast<std::chrono::seconds>(stop - start);
      std::cout << "  -> New Best Selection: " << best_selection.objective
                << " (" << duration << ")" << std::endl;
    }
    relinking.update_candidates(best_selection);
  };

  std::cout << "Phase 0:\n Initial: " << std::endl;
  res = run_initial_phase<InitialHeuristic, Intensification>(
      config.initial_solutions, config.int_config, config.type,
      config.termination_bound);
  res = com.exchange_best_selection(dgm, res);
  update_best_selection(res);
  print_result(best_selection.objective);

  TransformationHeuristic heuristic(dgm, config.type);
  TerminationCriterion termination_criterion(config.maxit,
                                             config.termination_bound);
  for (phase = 0;
       !termination_criterion.satisfied(phase, best_selection.objective);
       phase++) {
    std::cout << "Phase " << phase + 1 << ":" << std::endl;

    std::cout << " Diversify ";
    std::cout << "(Transformation):" << std::endl;
    heuristic.transform(config.diversification_rounds);
    res = run_intensification_phase<Intensification>(
        config.relinking_config.local_search_config, config.type,
        config.termination_bound);
    if (best_selection <= res && relinking.ready(config.relinking_config)) {
      std::cout << "(Relinking):" << std::endl;
      res = run_relinking_phase<Intensification>(
          config.relinking_config, config.type, best_selection.objective);
    } else {
      com.sync();
    }
    res = com.exchange_best_selection(dgm, res);
    update_best_selection(res, false);
    print_result(res.objective);

    if (termination_criterion.satisfied(phase, best_selection.objective))
      break;

    std::cout << " Intensify: " << std::endl;
    res = run_exhaustive_search<ExhaustiveSearch>(
        res, config.exhaustive_search_config, config.type);
    res = com.exchange_best_selection(dgm, res);
    update_best_selection(res);
    print_result(res.objective);
  }
  if (config.compress) {
    std::cout << " Compress: " << std::endl;
    res = run_compression_phase<Intensification>(best_selection,
                                                 config.int_config, config.type,
                                                 config.termination_bound);
    update_best_selection(res);
    print_result(res.objective);
  } else {
    dgm.restore_commit(*best_selection.commit_index);
    assert(
        (best_selection.objective == dgm.critical_path(config.type).objective));
  }
  std::cout << "Global Solution: " << best_selection.objective << std::endl;
}

template <class InitialHeuristic, class Intensification>
BestSelection TabuSearch::run_initial_phase(int initial_solutions,
                                            IntensificationConfig &config,
                                            CriticalPath::Objective type,
                                            Delay termination_bound) {
  assert((*this->best_selection.commit_index != config.commit_index));

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

  auto shuffle_and_restart = [&](auto e) {
    dgm.complete_shuffle(e, false);
    // start from beginning
    int_phase.reset_phase();
    best_selection =
        BestSelection(&config.commit_index, dgm.critical_path(type).objective);
    dgm.commit_all(config.commit_index);
    best_selection.committed = true;
  };

  size_t iteration;
  for (iteration = 0; !int_phase.completed(iteration, next_selection.objective);
       iteration++) {
    next_selection = int_phase.compute_next_selection(type);

    if (next_selection < best_selection) {
      update_best_selection(best_selection, next_selection);
      std::cout << iteration << " (" << best_selection.objective << ") ";
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
          shuffle_and_restart(e.required_shuffle);
        } else {
          continue;
        }
      }
    } else {
      shuffle_and_restart(next_selection.edges);
    }
  }

  // best_selection might not be committed if next_selection.objective ==
  // best_selection.objective for maxit iterations
  if (!best_selection.committed) {
    dgm.commit_all(*best_selection.commit_index);
    best_selection.committed = true;
  }

  std::cout << std::endl << "res " << best_selection.objective << std::endl;
  return best_selection;
}

template <class Intensification>
BestSelection TabuSearch::run_exhaustive_search(BestSelection &int_phase,
                                                ExhaustiveSearchConfig &config,
                                                CriticalPath::Objective type,
                                                Delay termination_bound) {
  assert((*best_selection.commit_index != config.commit_index));
  assert((*int_phase.commit_index != config.commit_index));

  BestSelection res, best_selection(&config.best_commit_index);
  SelectionCriticalBlockNeighborhood selection_neighborhood(dgm);
  ExhaustiveSearchTransformation transform(dgm, type);
  auto neighborhood =
      selection_neighborhood.compute(this->dgm.critical_path(type))
          .flip_candidates;
  auto partition = com.partition(neighborhood.begin(), neighborhood.end());
  Communicator::State state = Communicator::running;

  for (auto it = partition.first; it != partition.second; ++it) {
    auto edges = *it;

    // edge descriptors in neighborhood might be invalidated after restoring
    for (auto &e : edges)
      e = dgm.edge(e);

    this->dgm.complete_flip(edges);
    transform.compute_best_permutation(
        target(edges.front(), dgm.shuffle_graph));
    res = run_intensification_phase<Intensification>(config, type,
                                                     termination_bound);
    if (res < best_selection)
      update_best_selection(best_selection, res);
    state = best_selection < this->best_selection ? Communicator::found_better
                                                  : Communicator::running;
    state = com.exchange_state(state);
    if (state != Communicator::running)
      return best_selection;
    dgm.restore_commit(*int_phase.commit_index);
    assert((int_phase.objective == dgm.critical_path(type).objective));
  }
  if (state == Communicator::running)
    com.sync();

  return best_selection;
}

template <class Intensification>
BestSelection TabuSearch::run_relinking_phase(RelinkingConfig &config,
                                              CriticalPath::Objective type,
                                              Delay bound) {
  assert((config.best_commit_index != config.local_search_config.commit_index));
  assert(
      (config.backup_commit_index != config.local_search_config.commit_index));

  BestSelection best_selection(&config.best_commit_index);

  EncodedSelection &initial = relinking.sample(config, config.p_initial);
  dgm.decode(initial.buf);

  // For the guiding selection, we either use the solution from the previous
  // transformation phase, or another stored solution (depending on which one is
  // better)
  EncodedSelection guiding(dgm, type);
  EncodedSelection &guiding_alt = relinking.sample(config, config.p_guiding);
  if (initial.objective != guiding_alt.objective &&
      guiding_alt.objective < guiding.objective) {
    std::cout << "replaced: " << guiding.objective << " "
              << guiding_alt.objective << std::endl;
    guiding = guiding_alt;
  }

  auto differing_machines =
      relinking.compute_differing_machines(initial, guiding);
  int steps = config.min_separation;
  Communicator::State state = Communicator::running;
  for (int it = 0;; it++) {
    bool success = relinking.step_towards(guiding, differing_machines, steps);
    if (!success)
      break;
    Delay backup = dgm.critical_path(type).objective;
    dgm.commit_all(config.backup_commit_index);

    BestSelection res = run_intensification_phase<Intensification>(
        config.local_search_config, type);
    state = res < this->best_selection ? Communicator::found_better
                                       : Communicator::running;
    state = com.exchange_state(state);
    if (state == Communicator::found_better) {
      return res;
    } else if (state == Communicator::terminated) {
      return best_selection;
    } else if (res < best_selection && relinking.differs(res, guiding) &&
               relinking.differs(res, initial)) {
      steps = config.min_separation;
      update_best_selection(best_selection, res);
    } else {
      steps *= 2;
    }

    dgm.restore_commit(config.backup_commit_index);
    assert((backup == dgm.critical_path(type).objective));
  }
  if (state == Communicator::running)
    com.sync();
  return best_selection;
}

template <class Intensification>
BestSelection TabuSearch::run_compression_phase(BestSelection &best_selection,
                                                IntensificationConfig &config,
                                                CriticalPath::Objective type,
                                                Delay termination_bound) {
  assert((*best_selection.commit_index != config.commit_index));

  config.recursive_shuffle = true;

  BestSelection res = best_selection;
  CompressionNeighborhood compression_neighborhood(dgm);
  bool improvement_found = true;
  while (improvement_found) {
    improvement_found = false;
    dgm.restore_commit(*best_selection.commit_index);
    assert((best_selection.objective == dgm.critical_path(type).objective));

    auto &neighborhood =
        compression_neighborhood.compute(dgm.critical_path(type))
            .shuffle_candidates;
    auto partition = com.partition(neighborhood.begin(), neighborhood.end());
    Communicator::State state = Communicator::running;

    for (auto it = partition.first; it != partition.second; ++it) {
      auto edges = *it;

      // edge descriptors in neighborhood might be invalidated after restoring
      for (auto &e : edges)
        e = dgm.edge(e);

      try {
        dgm.complete_shuffle(edges);
        res = run_intensification_phase<Intensification>(config, type,
                                                         termination_bound);
      } catch (UnfixableCycleException &e) {
        continue;
      } catch (JitterBoundViolation &e) {
        continue;
      }
      state = res < best_selection ? Communicator::found_better
                                   : Communicator::running;
      state = com.exchange_state(state);
      if (state != Communicator::running)
        break;

      dgm.undo_last_shuffle();
    }
    if (state == Communicator::running)
      com.sync();

    res = com.exchange_best_selection(dgm, res);
    if (res < best_selection) {
      update_best_selection(best_selection, res);
      improvement_found = true;
      auto stop = std::chrono::high_resolution_clock::now();
      auto duration = duration_cast<std::chrono::seconds>(stop - start);
      std::cout << "  -> New Compressed Selection: " << best_selection.objective
                << " (" << duration << ")" << std::endl;
    }
  }

  config.recursive_shuffle = false;
  return best_selection;
}

} // namespace tsndgm

#endif // TSN_DGM_TABUSEARCH_TCC
