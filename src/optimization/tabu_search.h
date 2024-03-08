#ifndef TSN_DGM_TABUSEARCH_H
#define TSN_DGM_TABUSEARCH_H

#include "../dgm/dgm.h"
#include "communicator.h"
#include "intensification.h"
#include "selection.h"
#include "selection_storage.h"
#include "termination.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>

namespace tsndgm {
struct DiversificationConfig {
  size_t maxit;                    //!< number of iterations
  size_t max_stored_solutions = 5; //!< stores the best known selections
  double T_0 = 0.99;               // initial temperature
  double c = 0.3; // additional configuration parameter for temperature schedule
};

struct CompressionConfig {
  bool enabled = false;
  TerminationConfig tconfig;     //!< timeout config
  IntensificationConfig iconfig; //!< intensification config
  int sync_gap = 5; //!< how often processes synchronize their storage [seconds]
};

struct TabuSearchConfig {
  CriticalPath::Objective type;  //!< makespan or tardiness
  TerminationConfig tconfig;     //!< termination config
  IntensificationConfig iconfig; //!< intensification config
  DiversificationConfig dconfig; //!< diversification config
  CompressionConfig cconfig;     //!< ZIPS (false) or FIPS (true)
  int sync_gap = 5; //!< how often processes synchronize their storage [seconds]
  size_t initial_solutions = 1; //!< how often to run initial heuristic
};

class TabuSearch {
public:
  TabuSearch(const std::shared_ptr<NetworkTopology> &network,
             const std::vector<MessageStream> &streams)
      : dgm(network, streams), gen(rd()), storage(&dgm),
        compressed_storage(&dgm), com(),
        best_selection(std::numeric_limits<Delay>::max()) {
    reset_timer();
    log = std::ofstream("output_rank" + std::to_string(com.rank) + ".log");
    log.precision(3);
  }

  TabuSearch(DisjunctiveGraphModel &dgm)
      : dgm(dgm), gen(rd()), storage(&dgm), compressed_storage(&dgm),
        best_selection(std::numeric_limits<Delay>::max()) {
    reset_timer();
    log = std::ofstream("output_rank" + std::to_string(com.rank) + ".log");
    log.precision(3);
  }

  TabuSearch(const TabuSearch &other)
      : dgm(other.dgm), gen(rd()), storage(&this->dgm, other.storage),
        compressed_storage(&this->dgm, other.compressed_storage),
        com(other.com), best_selection(other.best_selection),
        start(other.start) {
    log = std::ofstream("output_rank" + std::to_string(com.rank) + ".log",
                        std::ios_base::app);
    log.precision(3);
  }

  template <class InitialHeuristic, class TerminationCriterion,
            class Intensification, class TransformationHeuristic>
  void run(TabuSearchConfig &config,
           std::map<MessageStreamHandle, RTIMap> rti_updates = {});

  template <class InitialHeuristic, class Intensification>
  EncodedSelection
  run_initial_phase(int initial_solutions, IntensificationConfig &config,
                    CriticalPath::Objective type, Delay termination_bound = 0);

  template <class Intensification>
  EncodedSelection run_intensification_phase(IntensificationConfig &config,
                                             CriticalPath::Objective type,
                                             Delay termination_bound = 0,
                                             TabuList tabu_list = {});

  template <class Intensification, class TerminationCriterion>
  void run_compression_phase(CompressionConfig &config,
                             CriticalPath::Objective type,
                             Delay termination_bound);

  void reset_timer() { start = std::chrono::high_resolution_clock::now(); }

  void update_rti(std::map<MessageStreamHandle, RTIMap> rti_updates,
                  CriticalPath::Objective type);

  DisjunctiveGraphModel dgm;
  Delay best_selection;
  Communicator com;

private:
  SelectionStorage storage, compressed_storage;
  std::random_device rd;
  std::mt19937 gen;
  std::ofstream log;

  void update_storage(SelectionStorage &storage, EncodedSelection &res,
                      bool sync = true);
  void update_storage(SelectionStorage &storage, bool sync = true);

  void print_result(Delay res) {
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::seconds>(stop - start);
    log << " Result: " << res << " (" << duration << ") " << std::endl;
  }

  void print_time() {
    auto stop = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::seconds>(stop - start);
    std::cout << duration << std::endl;
  }

  bool time_to_sync(auto &config);

  std::chrono::high_resolution_clock::time_point start;
  std::chrono::high_resolution_clock::time_point last_sync;
};

} // namespace tsndgm

#include "tabu_search.tcc"

#endif // TSN_DGM_TABUSEARCH_H
