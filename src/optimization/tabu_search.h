#ifndef TSN_DGM_TABUSEARCH_H
#define TSN_DGM_TABUSEARCH_H

#include "../dgm/dgm.h"
#include "communicator.h"
#include "intensification.h"
#include "selection.h"
#include "selection_storage.h"
#include "termination.h"
#include <chrono>
#include <random>

namespace tsndgm {
struct DiversificationConfig {
  size_t maxit;                     //!< number of iterations
  size_t max_stored_solutions = 10; //!< stores the best known selections
  double T_0 = 0.99;                // initial temperature
  double c = 0.3; // additional configuration parameter for temperature schedule
  size_t commit_index = 2; //!< where to store best selection of maxit rounds
};

struct CompressionConfig {
  bool enabled = false;
  TerminationConfig tconfig;     //!< timeout config
  IntensificationConfig iconfig; //!< intensification config
};

struct TabuSearchConfig {
  CriticalPath::Objective type;  //!< makespan or tardiness
  TerminationConfig tconfig;     //!< termination config
  IntensificationConfig iconfig; //!< intensification config
  DiversificationConfig dconfig; //!< diversification config
  CompressionConfig cconfig;     //!< ZIPS (false) or FIPS (true)
  size_t initial_solutions = 1;  //!< how often to run initial heuristic
  size_t commit_index = 0;       //!< where to store the global best selection
};

class TabuSearch {
public:
  TabuSearch(const std::shared_ptr<NetworkTopology> &network,
             const std::vector<MessageStream> &streams,
             bool multithreading = true)
      : dgm(network, streams), gen(rd()), storage(&dgm),
        compressed_storage(&dgm), com(multithreading) {
    reset_timer();
    std::cout.precision(3);
  }

  TabuSearch(DisjunctiveGraphModel &dgm, bool multithreading = true)
      : dgm(dgm), gen(rd()), storage(&dgm), compressed_storage(&dgm),
        com(multithreading) {
    reset_timer();
    std::cout.precision(3);
  }

  template <class InitialHeuristic, class TerminationCriterion,
            class Intensification, class TransformationHeuristic>
  void run(TabuSearchConfig &config,
           std::map<MessageStreamHandle, RTIMap> rti_updates = {});

  template <class InitialHeuristic, class Intensification>
  BestSelection
  run_initial_phase(int initial_solutions, IntensificationConfig &config,
                    CriticalPath::Objective type, Delay termination_bound = 0);

  template <class Intensification>
  BestSelection run_intensification_phase(IntensificationConfig &config,
                                          CriticalPath::Objective type,
                                          Delay termination_bound = 0,
                                          TabuList tabu_list = {});

  template <class Intensification, class TerminationCriterion>
  void run_compression_phase(CompressionConfig &config,
                             CriticalPath::Objective type,
                             Delay termination_bound);

  void reset_timer() { start = std::chrono::high_resolution_clock::now(); }

  DisjunctiveGraphModel dgm;
  BestSelection best_selection;
  Communicator com;

private:
  SelectionStorage storage, compressed_storage;
  std::random_device rd;
  std::mt19937 gen;

  void update_best_selection(BestSelection &best_selection, NextSelection &res);
  void update_best_selection(BestSelection &best_selection, BestSelection &res,
                             bool swap = true);
  void update_best_selection(SelectionStorage &storage, BestSelection &res,
                             bool swap = true);
  void update_best_selection(SelectionStorage &storage);

  void print_result(Delay res) {
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::seconds>(stop - start);
    std::cout << " Result: " << res << " (" << duration << ") " << std::endl;
  }

  std::chrono::high_resolution_clock::time_point start;
};

} // namespace tsndgm

#include "tabu_search.tcc"

#endif // TSN_DGM_TABUSEARCH_H
