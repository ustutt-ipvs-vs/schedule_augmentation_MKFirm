#ifndef TSN_DGM_TABUSEARCH_TCC
#define TSN_DGM_TABUSEARCH_TCC

#include "tabu_search.h"

namespace tsndgm {

template <class InitialHeuristic, class TerminationCriterion,
          class Intensification, class TransformationHeuristic>
void TabuSearch::run(TabuSearchConfig &config) {
  best_selection = BestSelection(&config.commit_index);
  SelectionStorage storage(dgm, config.dconfig.max_stored_solutions);
  TerminationCriterion termination_criterion(config.tconfig);
  BestSelection res;
  size_t phase;

  config.iconfig.maxt += com.rank;

  // compute initial solution
  std::cout << "Phase 0:\n Initial: " << std::endl;
  res = run_initial_phase<InitialHeuristic, Intensification>(
      config.initial_solutions, config.iconfig, config.type,
      config.tconfig.bound);
  update_best_selection(storage, res);
  com.signal_sync_storage(storage);
  print_result(best_selection.objective);

  int N = termination_criterion.progress(0, best_selection.objective).second;
  TransformationHeuristic heuristic(dgm, storage, config.type,
                                    config.dconfig.T_0, config.dconfig.c, N);
  for (phase = 0;
       !termination_criterion.satisfied(phase, best_selection.objective);
       phase++) {
    std::cout << "Phase " << phase + 1 << ":" << std::endl;
    // randomly select one of the best stored selections
    EncodedSelection &next = storage.sample();
    dgm.decode(next.buf);
    assert((next.objective == dgm.critical_path(config.type).objective));

    // transform solution to break out of local minima
    int progress =
        termination_criterion.progress(phase, best_selection.objective).first;
    heuristic.update_temperature(progress);
    int rounds = heuristic.transform(config.dconfig.maxit);
    std::cout << " Temperature: " << heuristic.temperature
              << "; Diversify Rounds: " << rounds
              << "; Total Flips: " << dgm.total_flips << std::endl;
    if (rounds == 0)
      continue;

    // intensify search to improve transformed solution
    res = run_intensification_phase<Intensification>(
        config.iconfig, config.type, config.tconfig.bound,
        create_tabu_list(heuristic.flipped_edges));
    update_best_selection(storage, res);
    com.signal_sync_storage(storage);
    print_result(res.objective);
  }
  com.sync(Communicator::State::terminated, 1);
  com.stop_sync_storage();

  // compress solution by shuffling operations
  if (config.compress) {
    std::cout << " Compress: " << std::endl;
    res = run_compression_phase<Intensification>(
        best_selection, config.iconfig, config.type, config.tconfig.bound);
    update_best_selection(storage, res);
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
BestSelection TabuSearch::run_intensification_phase(
    IntensificationConfig &config, CriticalPath::Objective type,
    Delay termination_bound, TabuList tabu_list) {
  Intensification int_phase(dgm, config, termination_bound);
  int_phase.tabu_list = tabu_list;
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

    if (next_selection <= best_selection) {
      update_best_selection(best_selection, next_selection);
    } else if (!best_selection.committed) {
      // commit best known solution if we start with uphill moves
      assert((best_selection.objective == dgm.critical_path(type).objective));
      dgm.commit_all(*best_selection.commit_index);
      best_selection.committed = true;
    }

    if (next_selection.operation == flip) {
      try {
        dgm.complete_flip(next_selection.edges);
        assert((next_selection.objective == dgm.critical_path(type).objective));
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

void TabuSearch::update_best_selection(BestSelection &best_selection,
                                       NextSelection &res) {
  best_selection.objective = res.objective;
  best_selection.committed = false;
}

void TabuSearch::update_best_selection(BestSelection &best_selection,
                                       BestSelection &res, bool swap) {
  best_selection.objective = res.objective;
  if (swap)
    std::swap(*best_selection.commit_index, *res.commit_index);
  else
    dgm.copy_commit(*res.commit_index, *best_selection.commit_index);

  best_selection.committed = true;
}

void TabuSearch::update_best_selection(SelectionStorage &storage,
                                       BestSelection &res, bool swap) {
  storage.update_candidates(res);
  auto print_new_best = [&]() {
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::seconds>(stop - start);
    std::cout << " New Best Selection: " << best_selection.objective << " ("
              << duration << ")" << std::endl;
  };
  if (storage.encoded_best_selections[0].objective <
      std::min(res.objective, best_selection.objective)) {
    dgm.decode(storage.encoded_best_selections[0].buf);
    dgm.commit_all(*best_selection.commit_index);
    best_selection.objective = storage.encoded_best_selections[0].objective;
    res.objective = best_selection.objective;
    res.committed = false;
    print_new_best();
  }
  if (res < best_selection) {
    this->update_best_selection(best_selection, res, swap);
    print_new_best();
  }
}

} // namespace tsndgm

#endif // TSN_DGM_TABUSEARCH_TCC
