#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <nlohmann/json.hpp>

#include "../src/dgm/dgm.h"
#include "setup.h"

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

  Delay zips_bound = optimal_makespan;

  while (true) {
    int retries, max_retries = 100;
    for (retries = 0; retries < max_retries; retries++) {
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

        // now, set d_min to zero
        RTIMap new_rti = {
            {shuffle_graph[v].edge, RTI(old_rti.d_trans_max() + slack, 0)}};
        dgm.update_rti(shuffle_graph[v].ms_handle.front(), new_rti);
        res = dgm.critical_path(CriticalPath::Objective::makespan);

        if (res.objective == optimal_makespan) {
          N++;
          continue;
        }
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
                config, CriticalPath::Objective::makespan, optimal_makespan);
        std::cout << "Result: " << compressed_selection.objective << std::endl;
        if (compressed_selection.objective <= optimal_makespan) {
          std::cout << " -> eligible for benchmarking" << std::endl;
          dgm.restore_commit(*tabu_search.best_selection.commit_index);
          zips_bound = res.objective;
        } else {
          std::cout << " -> ineligible for benchmarking" << std::endl;
          N++;
          dgm.restore_commit(3);
        }
      }
      // if N <= N_max, we were successful
      if (N <= N_max) {
        std::cout << "next stage" << std::endl;
        break;
      }
    }
    if (retries == max_retries) {
      break;
    } else {
      print(tabu_search.dgm, jsp.machine_to_datalink);
      save(dgm, jsp.machine_to_datalink, "converted",
           benchmark_data["name"].template get<std::string>());
      save_instance(dgm, benchmark_data["name"].template get<std::string>());
    }
  }

  std::cout << "\n\nGenerated Benchmark:" << std::endl;
  std::cout << "ZIPS: (LB, UB) = (" << optimal_makespan << ", " << zips_bound
            << ")" << std::endl;
  std::cout << "FIPS: (LB, UB) = (" << optimal_makespan << ", "
            << optimal_makespan << ")" << std::endl;
  print(tabu_search.dgm, jsp.machine_to_datalink);
}
