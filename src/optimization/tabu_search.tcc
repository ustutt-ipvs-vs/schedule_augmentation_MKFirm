#ifndef TSN_DGM_TABUSEARCH_TCC
#define TSN_DGM_TABUSEARCH_TCC

#include "tabu_search.h"

namespace tsndgm {

template <class InitialHeuristic, class TerminationCriterion,
          class Intensification, class TransformationHeuristic>
void TabuSearch::run(TabuSearchConfig &config,
                     std::map<MessageStreamHandle, RTIMap> rti_updates) {
  log << "\n----------------------------------------------" << std::endl;
  log << "                 Solver (ZIPS)                " << std::endl;
  log << "----------------------------------------------" << std::endl;

  best_selection = std::numeric_limits<Delay>::max();
  storage.set_capacity(config.dconfig.max_stored_solutions);
  compressed_storage.set_capacity(config.dconfig.max_stored_solutions);
  dgm.update_rti(rti_updates);

  TerminationCriterion termination_criterion(config.tconfig);
  EncodedSelection res;
  size_t phase;

  // compute initial solution
  if (storage.size() == 0) {
    log << "Phase 0:\n Initial: " << std::endl;
    res = run_initial_phase<InitialHeuristic, Intensification>(
        config.initial_solutions, config.iconfig, config.type,
        config.tconfig.bound);
    sync.start();
    update_storage(storage, res);
    print_result(best_selection);
  } else {
    sync.start();
    if (compressed_storage.size() > 0) {
      compressed_storage.renew_storage_objectives(config.type);
      best_selection = compressed_storage.best().objective;
    }
    storage.renew_storage_objectives(config.type);
    best_selection = std::min(best_selection, storage.best().objective);
  }

  int N = termination_criterion.progress(0, best_selection).second;
  TransformationHeuristic heuristic(dgm, storage, config.type,
                                    config.dconfig.T_0, config.dconfig.c, N);
  std::uniform_int_distribution<> d(0, config.dconfig.maxit);
  for (phase = 0; !termination_criterion.satisfied(phase, best_selection);
       phase++) {
    log << "Phase " << phase + 1 << ":" << std::endl;
    // randomly select one of the best stored selections
    EncodedSelection &next = storage.sample(heuristic.temperature);
    dgm.decode(next.buf);

    // transform solution to break out of local minima
    int progress = termination_criterion.progress(phase, best_selection).first;
    heuristic.update_temperature(progress);
    int rounds = heuristic.transform(d(gen));
    log << " Temperature: " << heuristic.temperature
        << "; Diversify Rounds: " << rounds
        << "; Total Flips: " << dgm.total_flips
        << "; Total Traversals: " << longest_path_visitor::total_traversals
        << std::endl;
    log << " Storage: ";
    for (auto &stored_selection : storage.encoded_best_selections) {
      log << stored_selection.objective << " ";
    }
    log << std::endl;
    if (rounds == 0)
      continue;

    // intensify search to improve transformed solution
    res = run_intensification_phase<Intensification>(
        config.iconfig, config.type, config.tconfig.bound,
        create_tabu_list(heuristic.flipped_edges));
    update_storage(storage, res, time_to_sync(config));
    print_result(res.objective);
  }

  // compress solution by shuffling operations
  if (config.cconfig.enabled) {
    log << "----------------------------------------------" << std::endl;
    log << "          Compression (ZIPS -> FIPS)          " << std::endl;
    log << "----------------------------------------------" << std::endl;
    reset_timeout();
    run_compression_phase<Intensification, TerminationCriterion>(
        config.cconfig, config.type, config.tconfig.bound);
    sync.stop(compressed_storage);
    update_storage(compressed_storage);
    dgm.decode(compressed_storage.best().buf);
  } else {
    sync.stop(storage);
    update_storage(storage);
    dgm.decode(storage.best().buf);
  }

  log << "Global Solution: " << dgm.critical_path(config.type).objective
      << std::endl;
}

template <class InitialHeuristic, class Intensification>
EncodedSelection TabuSearch::run_initial_phase(int initial_solutions,
                                               IntensificationConfig &config,
                                               CriticalPath::Objective type,
                                               Delay termination_bound) {
  EncodedSelection best;
  InitialHeuristic initial_heuristic(dgm, type);
  for (int i = 0; i < initial_solutions; i++) {
    initial_heuristic.generate();
    EncodedSelection res = run_intensification_phase<Intensification>(
        config, type, termination_bound);
    if (res.objective < best.objective) {
      std::swap(res, best);
    }
    Communicator::State state = res.objective <= termination_bound
                                    ? Communicator::found_better
                                    : Communicator::running;
    state = com.exchange_state(state);
    if (state != Communicator::running)
      return best;
  }
  com.sync();

  return best;
}

template <class Intensification>
EncodedSelection TabuSearch::run_intensification_phase(
    IntensificationConfig &config, CriticalPath::Objective type,
    Delay termination_bound, TabuList tabu_list) {
  Intensification int_phase(dgm, config, termination_bound);
  int_phase.tabu_list = tabu_list;
  EncodedSelection best(dgm, type);

  auto shuffle_and_restart = [&](auto e) {
    dgm.complete_shuffle(e, false);
    // start from beginning
    int_phase.reset_phase();
    best = EncodedSelection(dgm, type);
  };

  for (size_t it = 0; !int_phase.completed(it, best.objective); it++) {
    auto next_selection = int_phase.compute_next_selection(type);
    if (next_selection.edges.size() == 0) {
      // if there is no disjunctive edge on the CS, we reached the optimum
      return EncodedSelection(dgm, type);
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

      if (dgm.critical_path(type).objective < best.objective)
        best = EncodedSelection(dgm, type);
    } else {
      shuffle_and_restart(next_selection.edges);
    }
  }

  return best;
}

template <class Intensification, class TerminationCriterion>
void TabuSearch::run_compression_phase(CompressionConfig &config,
                                       CriticalPath::Objective type,
                                       Delay termination_bound) {
  TerminationCriterion termination_criterion(config.tconfig);
  WirelessCompressionNeighborhood compression_neighborhood(dgm);
  double temperature = 0;
  config.iconfig.recursive_shuffle = true;

  for (auto &stored_selection : storage.encoded_best_selections) {
    compressed_storage.update_candidates(stored_selection);
  }

  for (size_t phase = 0;
       !termination_criterion.satisfied(phase, best_selection); phase++) {
    log << "Phase " << phase + 1 << ":" << std::endl;
    log << " Storage (ZIPS): ";
    for (auto &stored_selection : storage.encoded_best_selections) {
      log << stored_selection.objective << " ";
    }
    log << std::endl << " Storage (FIPS): ";
    for (auto &stored_selection : compressed_storage.encoded_best_selections) {
      log << stored_selection.objective << " ["
          << stored_selection.extension_level;
      if (stored_selection.neighborhood.has_value())
        log << ", " << stored_selection.neighborhood->shuffle_candidates.size();
      log << "] ";
    }
    log << std::endl;

    EncodedSelection &next = compressed_storage.sample(temperature);
    dgm.decode(next.buf);

    if (!next.neighborhood.has_value()) {
      next.neighborhood = compression_neighborhood.extend(
          dgm.critical_path(type), next.extension_level);
      next.extension_level++;
    }

    size_t k;
    EncodedSelection res;
    auto &neighborhood = next.neighborhood->shuffle_candidates;
    for (k = 0; k < neighborhood.size(); ++k) {
      std::uniform_int_distribution<size_t> d(k, neighborhood.size() - 1);
      size_t i = d(gen);
      neighborhood[k].swap(neighborhood[i]);
      auto edges = neighborhood[k];

      // edge descriptors in neighborhood might be invalidated after restoring
      for (auto &e : edges)
        e = dgm.edge(e);

      try {
        dgm.complete_shuffle(edges);
        res = run_intensification_phase<Intensification>(config.iconfig, type,
                                                         termination_bound);
        if (res.objective <= next.objective)
          break;
      } catch (std::exception &e) {
        res.objective = std::numeric_limits<Delay>::max();
      }

      dgm.undo_last_shuffle();
    }

    print_result(res.objective);
    if (k < neighborhood.size()) {
      neighborhood.erase(neighborhood.begin(), neighborhood.begin() + k + 1);
      temperature = res.objective < best_selection ? 1 : 0;
      update_storage(compressed_storage, res, time_to_sync(config));
    } else if (next.extension_level <=
               WirelessCompressionNeighborhood::max_extension) {
      next.neighborhood = {};
      update_storage(compressed_storage, time_to_sync(config));
      temperature = 0;
    } else {
      compressed_storage.delete_candidate(&next);
      update_storage(compressed_storage, time_to_sync(config));
      temperature = 0;
    }
  }
}

void TabuSearch::update_storage(SelectionStorage &storage,
                                EncodedSelection &res, bool sync) {
  auto print_new_best = [&]() {
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::seconds>(stop - start);
    log << " New Best Selection: " << best_selection << " (" << duration << ")"
        << std::endl;
  };

  storage.update_candidates(res);
  if (sync) {
    this->sync.update(storage);
    last_sync = std::chrono::high_resolution_clock::now();
  }

  if (storage.best().objective < best_selection) {
    best_selection = storage.best().objective;
    print_new_best();
  }
}

void TabuSearch::update_storage(SelectionStorage &storage, bool sync) {
  EncodedSelection dummy;
  update_storage(storage, dummy, sync);
}

void TabuSearch::update_rti(std::map<MessageStreamHandle, RTIMap> rti_updates,
                            CriticalPath::Objective type) {
  dgm.update_rti(rti_updates);
  EncodedSelection *best;
  if (compressed_storage.size() > 0) {
    compressed_storage.renew_storage_objectives(type);
    best = &compressed_storage.best();
    dgm.decode(best->buf);
  } else if (storage.size() > 0) {
    storage.renew_storage_objectives(type);
    best = &storage.best();
    dgm.decode(best->buf);
  }
}

bool TabuSearch::time_to_sync(auto &config) {
  if (last_sync < start) {
    last_sync = start;
    return true;
  } else if (best_selection <= config.tconfig.bound) {
    return true;
  }

  auto stop = std::chrono::high_resolution_clock::now();
  auto duration = duration_cast<std::chrono::seconds>(stop - last_sync);
  return duration >= static_cast<std::chrono::seconds>(config.sync_gap);
}

} // namespace tsndgm

#endif // TSN_DGM_TABUSEARCH_TCC
