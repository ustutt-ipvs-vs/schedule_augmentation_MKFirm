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
  size_t maxit;                    //!< number of iterations
  size_t max_stored_solutions = 4; //!< stores the best known selections
  double T_0 = 0.99;               // initial temperature
  double c = 0.3; // additional configuration parameter for temperature schedule
  size_t commit_index = 2; //!< where to store best selection of maxit rounds
};

struct TabuSearchConfig {
  CriticalPath::Objective type;  //!< makespan or tardiness
  TerminationConfig tconfig;     //!< termination config
  IntensificationConfig iconfig; //!< intensification config
  DiversificationConfig dconfig; //!< diversification config
  size_t initial_solutions = 1;  //!< how often to run initial heuristic
  bool compress = false;         //!< ZIPS (false) or FIPS (true)
  size_t commit_index = 0;       //!< where to store the global best selection
};

class TabuSearch {
public:
  TabuSearch(const std::shared_ptr<NetworkTopology> &network,
             const std::vector<MessageStream> &streams)
      : dgm(network, streams), gen(rd()) {
    start = std::chrono::high_resolution_clock::now();
    std::cout.precision(3);
  }

  TabuSearch(DisjunctiveGraphModel &dgm) : dgm(dgm), gen(rd()) {
    start = std::chrono::high_resolution_clock::now();
    std::cout.precision(3);
  }

  template <class InitialHeuristic, class TerminationCriterion,
            class Intensification, class TransformationHeuristic>
  void run(TabuSearchConfig &config);

  template <class InitialHeuristic, class Intensification>
  BestSelection
  run_initial_phase(int initial_solutions, IntensificationConfig &config,
                    CriticalPath::Objective type, Delay termination_bound = 0);

  template <class Intensification>
  BestSelection run_intensification_phase(IntensificationConfig &config,
                                          CriticalPath::Objective type,
                                          Delay termination_bound = 0,
                                          TabuList tabu_list = {});

  template <class Intensification>
  BestSelection run_compression_phase(BestSelection &best_selection,
                                      IntensificationConfig &config,
                                      CriticalPath::Objective type,
                                      Delay termination_bound);

  DisjunctiveGraphModel dgm;
  BestSelection best_selection;
  Communicator com;

private:
  std::random_device rd;
  std::mt19937 gen;

  void update_best_selection(BestSelection &best_selection, NextSelection &res);
  void update_best_selection(BestSelection &best_selection, BestSelection &res,
                             bool swap = true);
  void update_best_selection(SelectionStorage &storage, BestSelection &res,
                             bool swap = true);

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
