#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <nlohmann/json.hpp>

#include "../src/dgm/dgm.h"
#include "setup.h"

#define MAX_RETRIES 5
#define EPSILON 0.1

typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

int main(int argc, char **argv) {
  if (argc < 2 || argc > 3) {
    std::cout << "Usage: ./jsp [0-161] [subdir]\ndefault: subdir='fifo_jsp'"
              << std::endl;
    exit(0);
  }
  int benchmark_id = stoi(argv[1]);

  std::ifstream f("../data/JSPLIB/instances.json");
  json data = json::parse(f);
  json benchmark_data = data[benchmark_id];

  JSPwithFIFOSetup jsp;
  jsp.setup(benchmark_data);

  using Intensification =
      TestStrictIntensification<DifferentialTerminationCriterion,
                                ReducedSelectionCriticalBlockNeighborhood>;
  IntensificationConfig config = {
      static_cast<size_t>(jsp.machines),
      static_cast<size_t>(10 * jsp.machines * jsp.jobs)};
  TabuSearch tabu_search(jsp.network, jsp.streams);
  auto &dgm = tabu_search.dgm;

  std::cout << benchmark_data["name"].template get<std::string>() << std::endl;
  if (argc == 2)
    jsp.set_initial_solution(
        dgm, benchmark_data["name"].template get<std::string>());
  else
    set_initial_solution(dgm, argv[2],
                         benchmark_data["name"].template get<std::string>());

  Delay optimal_makespan =
      dgm.critical_path(CriticalPath::Objective::makespan).objective;
  std::cout << "start: " << optimal_makespan << std::endl;

  size_t commit_index = 2;
  dgm.commit_all(commit_index);
  tabu_search.best_selection = BestSelection(&commit_index, optimal_makespan);

  shuffle_graph_t &shuffle_graph = dgm.shuffle_graph;
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  std::random_device rd;
  std::mt19937 gen(rd());

  Delay fips_bound = optimal_makespan;
  bool found = false, repeat = true;

  while (repeat) {
    repeat = false;
    int retries;
    dgm.commit_all(4);
    for (retries = 0; retries < MAX_RETRIES; retries++) {
      int N = 1, N_max = boost::num_vertices(shuffle_graph) - 2;
      std::set<V> modified_operations;
      for (int i = 0; i < std::min(N, N_max); i++) {
        std::uniform_int_distribution<> d(
            2, boost::num_vertices(shuffle_graph) - 1 - i);
        V v = d(gen);
        for (V u : modified_operations)
          if (u <= v)
            v++;
        modified_operations.insert(v);

        auto res = dgm.critical_path(CriticalPath::Objective::makespan);
        Delay slack = optimal_makespan - prop.crit_cost[v];
        res = dgm.critical_path(CriticalPath::Objective::makespan, false);
        slack -= prop.crit_cost[v];
        if (slack <= 0) {
          N++;
          continue;
        }

        RTI old_rti = prop.streams[shuffle_graph[v].ms_handle.front()]
                          .rti_map[shuffle_graph[v].edge];

        std::uniform_int_distribution<> ds(0, slack);
        slack = ds(gen);
        RTIMap new_rti = {
            {shuffle_graph[v].edge, RTI(old_rti.d_trans_max() + slack, 0)}};
        dgm.update_rti(shuffle_graph[v].ms_handle.front(), new_rti);
        res = dgm.critical_path(CriticalPath::Objective::makespan);

        // modification is uninteresting if we do not exceed the previous bound
        if (res.objective <= fips_bound) {
          N++;
          dgm.update_rti(shuffle_graph[v].ms_handle.front(),
                         {{shuffle_graph[v].edge, old_rti}});
          continue;
        }

        // run compression phase to check if there exists a better result
        std::cout << "\nInitial: " << res.objective << " " << commit_index
                  << " " << config.commit_index << std::endl;
        dgm.commit_all(3);
        dgm.commit_all(commit_index);
        tabu_search.best_selection =
            BestSelection(&commit_index, res.objective);
        tabu_search.best_selection.committed = true;
        std::cout << "Compression:" << std::endl;
        auto compressed_selection =
            tabu_search.run_compression_phase<Intensification>(
                tabu_search.best_selection, config,
                CriticalPath::Objective::makespan, optimal_makespan);
        std::cout << "Result: " << compressed_selection.objective << std::endl;

        // compressed result must be smaller than ZIPS objective and within
        // EPSILON bound of optimal selection
        if (compressed_selection.objective <
            std::min(static_cast<Delay>((1 + EPSILON) * optimal_makespan),
                     res.objective)) {
          dgm.restore_commit(*compressed_selection.commit_index);
          fips_bound = compressed_selection.objective;
          dgm.commit_all(4);
          found = true;
        } else {
          std::cout << " -> ineligible for benchmarking" << std::endl;
          N++;
          dgm.restore_commit(3);
          dgm.update_rti(shuffle_graph[v].ms_handle.front(),
                         {{shuffle_graph[v].edge, old_rti}});
        }
      }
      // if N <= N_max, we were successful
      if (N <= N_max) {
        break;
      } else {
        dgm.restore_commit(4);
      }
    }
  }
  if (!found) {
    std::cout << "no interesting benchmark was found" << std::endl;
    return 1;
  }

  dgm.restore_commit(4);
  dgm.split_all();
  Delay zips_bound =
      dgm.critical_path(CriticalPath::Objective::makespan).objective;
  if (zips_bound <= fips_bound) {
    std::cout << "no interesting benchmark was found: " << zips_bound << " "
              << fips_bound << std::endl;
    return 1;
  }
  std::cout << "\n\nGenerated Benchmark:" << std::endl;
  std::cout << "-------------------------------------------------" << std::endl;
  std::cout << "ZIPS: (LB, UB) = (" << optimal_makespan << ", " << zips_bound
            << ")" << std::endl;
  std::cout << "Witness:" << std::endl;
  print(dgm, jsp.machine_to_datalink);

  dgm.restore_commit(4);
  std::cout << "-------------------------------------------------" << std::endl;
  std::cout << "FIPS: (LB, UB) = (" << optimal_makespan << ", " << fips_bound
            << ")" << std::endl;
  std::cout << "Witness:" << std::endl;
  print(dgm, jsp.machine_to_datalink);

  save(dgm, jsp.machine_to_datalink, "converted",
       benchmark_data["name"].template get<std::string>());
  save_instance(dgm, benchmark_data["name"].template get<std::string>());

  return 0;
}
