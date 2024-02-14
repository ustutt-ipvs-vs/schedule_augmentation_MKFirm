#ifndef TSN_DGM_TABUSEARCH_TCC
#define TSN_DGM_TABUSEARCH_TCC

#include "tabu_search.h"

namespace tsndgm {

template <class InitialHeuristic, class TerminationCriterion,
          class Intensification, class TransformationHeuristic>
void TabuSearch::run(TabuSearchConfig &config,
                     std::map<MessageStreamHandle, RTIMap> rti_updates) {
  std::cout << "\n----------------------------------------------" << std::endl;
  std::cout << "                 Solver (ZIPS)                " << std::endl;
  std::cout << "----------------------------------------------" << std::endl;

  best_selection = BestSelection(&config.commit_index);
  storage.set_capacity(config.dconfig.max_stored_solutions);
  compressed_storage.set_capacity(config.dconfig.max_stored_solutions);
  dgm.update_rti(rti_updates);
  storage.renew_storage_objectives(config.type);
  compressed_storage.renew_storage_objectives(config.type);

  TerminationCriterion termination_criterion(config.tconfig);
  BestSelection res;
  size_t phase;

  // compute initial solution
  if (storage.size() == 0) {
    std::cout << "Phase 0:\n Initial: " << std::endl;
    res = run_initial_phase<InitialHeuristic, Intensification>(
        config.initial_solutions, config.iconfig, config.type,
        config.tconfig.bound);
    update_best_selection(storage, res);
    print_result(best_selection.objective);
  }

  int N = termination_criterion.progress(0, best_selection.objective).second;
  TransformationHeuristic heuristic(dgm, storage, config.type,
                                    config.dconfig.T_0, config.dconfig.c, N);
  std::uniform_int_distribution<> d(0, config.dconfig.maxit);
  for (phase = 0;
       !termination_criterion.satisfied(phase, best_selection.objective);
       phase++) {
    std::cout << "Phase " << phase + 1 << ":" << std::endl;
    // randomly select one of the best stored selections
    EncodedSelection &next = storage.sample(heuristic.temperature);
    dgm.decode(next.buf);
    assert(next.objective == dgm.critical_path(config.type).objective);

    // transform solution to break out of local minima
    int progress =
        termination_criterion.progress(phase, best_selection.objective).first;
    heuristic.update_temperature(progress);
    int rounds = heuristic.transform(d(gen));
    std::cout << " Temperature: " << heuristic.temperature
              << "; Diversify Rounds: " << rounds
              << "; Total Flips: " << dgm.total_flips << std::endl;
    std::cout << " Storage: ";
    for (auto &stored_selection : storage.encoded_best_selections) {
      std::cout << stored_selection.objective << " ";
    }
    std::cout << std::endl;
    if (rounds == 0)
      continue;

    // intensify search to improve transformed solution
    res = run_intensification_phase<Intensification>(
        config.iconfig, config.type, config.tconfig.bound,
        create_tabu_list(heuristic.flipped_edges));
    update_best_selection(storage, res);
    print_result(res.objective);
  }
  com.stop_sync_storage();
  update_best_selection(storage);

  // compress solution by shuffling operations
  if (config.cconfig.enabled) {
    reset_timeout();
    std::cout << "----------------------------------------------" << std::endl;
    std::cout << "          Compression (ZIPS -> FIPS)          " << std::endl;
    std::cout << "----------------------------------------------" << std::endl;
    config.cconfig.iconfig.commit_index = config.iconfig.commit_index;
    run_compression_phase<Intensification, TerminationCriterion>(
        config.cconfig, config.type, config.tconfig.bound);
    com.stop_sync_storage();
    update_best_selection(compressed_storage);
  }

  dgm.restore_commit(*best_selection.commit_index);
  assert(best_selection.objective == dgm.critical_path(config.type).objective);
  std::cout << "Global Solution: " << best_selection.objective << std::endl;
}

template <class InitialHeuristic, class Intensification>
BestSelection TabuSearch::run_initial_phase(int initial_solutions,
                                            IntensificationConfig &config,
                                            CriticalPath::Objective type,
                                            Delay termination_bound) {
  assert(*this->best_selection.commit_index != config.commit_index);

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

  for (size_t iteration = 0;
       !int_phase.completed(iteration, next_selection.objective); iteration++) {
    next_selection = int_phase.compute_next_selection(type);
    if (next_selection.edges.size() == 0)
      break;

    if (next_selection.operation == flip) {
      if (next_selection <= best_selection) {
        update_best_selection(best_selection, next_selection);
      } else if (!best_selection.committed) {
        // commit best known solution if we start with uphill moves
        assert(best_selection.objective == dgm.critical_path(type).objective);
        dgm.commit_all(*best_selection.commit_index);
        best_selection.committed = true;
      }

      try {
        dgm.complete_flip(next_selection.edges);
        assert(next_selection.objective == dgm.critical_path(type).objective);
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
  if (!best_selection.committed &&
      best_selection.objective >= dgm.critical_path(type).objective) {
    dgm.commit_all(*best_selection.commit_index);
    best_selection.objective = dgm.critical_path(type).objective;
    best_selection.committed = true;
  }

  return best_selection;
}

template <class Intensification, class TerminationCriterion>
void TabuSearch::run_compression_phase(CompressionConfig &config,
                                       CriticalPath::Objective type,
                                       Delay termination_bound) {
  assert(config.iconfig.commit_index != *best_selection.commit_index);

  TerminationCriterion termination_criterion(config.tconfig);
  WirelessCompressionNeighborhood compression_neighborhood(dgm);
  double temperature = 0;
  config.iconfig.recursive_shuffle = true;

  for (auto &stored_selection : storage.encoded_best_selections) {
    compressed_storage.update_candidates(stored_selection);
  }

  for (size_t phase = 0;
       !termination_criterion.satisfied(phase, best_selection.objective);
       phase++) {
    std::cout << "Phase " << phase + 1 << ":" << std::endl;
    std::cout << " Storage (ZIPS): ";
    for (auto &stored_selection : storage.encoded_best_selections) {
      std::cout << stored_selection.objective << " ";
    }
    std::cout << std::endl << " Storage (FIPS): ";
    for (auto &stored_selection : compressed_storage.encoded_best_selections) {
      std::cout << stored_selection.objective << " ["
                << stored_selection.extension_level;
      if (stored_selection.neighborhood.has_value())
        std::cout << ", "
                  << stored_selection.neighborhood->shuffle_candidates.size();
      std::cout << "] ";
    }
    std::cout << std::endl;

    EncodedSelection &next = compressed_storage.sample(temperature);
    dgm.decode(next.buf);
    if (dgm.critical_path(type).objective != next.objective) {
      compressed_storage.delete_candidate(&next);
      update_best_selection(compressed_storage);
      temperature = 0;
      continue;
    }
    std::cout << " 1" << std::endl;

    BestSelection res;
    if (!next.neighborhood.has_value()) {
      next.neighborhood = compression_neighborhood.extend(
          dgm.critical_path(type), next.extension_level);
      next.extension_level++;
    }
    std::cout << " 2" << std::endl;

    size_t k;
    auto &neighborhood = next.neighborhood->shuffle_candidates;
    for (k = 0; k < neighborhood.size(); ++k) {
      std::uniform_int_distribution<size_t> d(k, neighborhood.size() - 1);
      size_t i = d(gen);
      neighborhood[k].swap(neighborhood[i]);
      auto edges = neighborhood[k];

      std::cout << " 3" << std::endl;
      // edge descriptors in neighborhood might be invalidated after restoring
      for (auto &e : edges)
        e = dgm.edge(e);

      std::cout << " 4" << std::endl;
      try {
        dgm.complete_shuffle(edges);
        res = run_intensification_phase<Intensification>(config.iconfig, type,
                                                         termination_bound);
        if (res.objective < next.objective)
          break;
      } catch (std::exception &e) {
        res.objective = std::numeric_limits<Delay>::max();
      }
      std::cout << " 5" << std::endl;

      dgm.undo_last_shuffle();
      std::cout << " 6" << std::endl;
    }

    print_result(res.objective);
    if (k < neighborhood.size()) {
      std::cout << " 7" << std::endl;
      neighborhood.erase(neighborhood.begin(), neighborhood.begin() + k + 1);
      temperature = res < best_selection ? 1 : 0;
      update_best_selection(compressed_storage, res);
    } else if (next.extension_level <=
               WirelessCompressionNeighborhood::max_extension) {
      std::cout << " 8" << std::endl;
      next.neighborhood = {};
      update_best_selection(compressed_storage);
      temperature = 0;
    } else {
      std::cout << " 9" << std::endl;
      compressed_storage.delete_candidate(&next);
      update_best_selection(compressed_storage);
      temperature = 0;
    }
    std::cout << " 10" << std::endl;
  }
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
  auto print_new_best = [&]() {
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::seconds>(stop - start);
    std::cout << " New Best Selection: " << best_selection.objective << " ("
              << duration << ")" << std::endl;
  };

  storage.update_candidates(res);
  com.signal_sync_storage(storage);
  if (storage.encoded_best_selections[0].objective <
      std::min(res.objective, best_selection.objective)) {
    dgm.decode(storage.encoded_best_selections[0].buf);
    dgm.commit_all(*best_selection.commit_index);
    best_selection.objective = storage.encoded_best_selections[0].objective;
    res.objective = best_selection.objective;
    res.committed = false;
    print_new_best();
  } else if (res < best_selection) {
    this->update_best_selection(best_selection, res, swap);
    print_new_best();
  }
}

void TabuSearch::update_best_selection(SelectionStorage &storage) {
  update_best_selection(storage, best_selection);
}

} // namespace tsndgm

#endif // TSN_DGM_TABUSEARCH_TCC
