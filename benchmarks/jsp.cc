#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <nlohmann/json.hpp>

#include "setup.h"

using json = nlohmann::json;
using namespace std;
using namespace tsndgm;
namespace fs = std::filesystem;

template <class InitialHeuristic, class TerminationCriterion,
          class Intensification, class TransformationHeuristic>
void benchmark_instance(
    json &benchmark_data,
    const std::function<TabuSearchConfig(int, int)> &config) {

  JSPSetup jsp;
  jsp.setup(benchmark_data);

  auto c = config(jsp.machines, jsp.jobs);
  if (!benchmark_data["optimum"].is_null()) {
    c.tconfig.bound = benchmark_data["optimum"].template get<Delay>();
  } else {
    c.tconfig.bound = benchmark_data["bounds"]["lower"].template get<Delay>();
  }

  TabuSearch tabu_search(jsp.network, jsp.streams);
  DisjunctiveGraphModel &dgm = tabu_search.dgm;

  if (tabu_search.com.rank != 0)
    std::cout.setstate(std::ios::failbit);

  cout << benchmark_data["name"].template get<std::string>() << std::endl;

  tabu_search.run<InitialHeuristic, TerminationCriterion, Intensification,
                  TransformationHeuristic>(c);

  if (!benchmark_data["optimum"].is_null()) {
    cout << "Known Optimal Solution: " << benchmark_data["optimum"]
         << std::endl;
  } else {
    cout << "Known Optimal Solution: [" << benchmark_data["bounds"]["lower"]
         << ", " << benchmark_data["bounds"]["upper"] << "]" << std::endl;
  }

  print(tabu_search.dgm, jsp.machine_to_datalink);
  jsp.save(tabu_search.dgm, jsp.machine_to_datalink,
           benchmark_data["name"].template get<std::string>());
}

template <class InitialHeuristic, class TerminationCriterion,
          class Intensification, class TransformationHeuristic>
void benchmark(const std::function<TabuSearchConfig(int, int)> &config,
               int benchmark_id) {
  std::ifstream f("../data/JSPLIB/instances.json");
  json data = json::parse(f);

  auto start = std::chrono::high_resolution_clock::now();
  benchmark_instance<InitialHeuristic, TerminationCriterion, Intensification,
                     TransformationHeuristic>(data[benchmark_id], config);
  auto stop = std::chrono::high_resolution_clock::now();
  auto duration = duration_cast<std::chrono::seconds>(stop - start);
  cout << "Time: " << duration.count() << " s" << endl;
}

int main(int argc, char **argv) {
  if (argc != 6) {
    std::cout << "Usage: ./jsp [0-161] maxt timeout div maxit" << std::endl;
    exit(0);
  }

  int benchmark_id = stoi(argv[1]);
  int maxt = stoi(argv[2]);
  int timeout = stoi(argv[3]);
  int div = stoi(argv[4]);
  int maxit = stoi(argv[5]);

  auto config = [&](int machines, int jobs) {
    return TabuSearchConfig{
        CriticalPath::Objective::makespan, TerminationConfig(timeout),
        IntensificationConfig(maxt, maxit * machines * jobs),
        DiversificationConfig(div)};
  };

  using InitialHeuristic = RandomInitial;
  using TerminationCriterion = TimeoutTerminationCriterion;
  using Intensification = StrictAdmissionIntensification<
      DifferentialTerminationCriterion,
      ReducedSelectionCriticalBlockNeighborhood<1>>;
  using TransformationHeuristic =
      RandomCriticalPathTransformation<ConstantThenSlowTemperature>;

  benchmark<InitialHeuristic, TerminationCriterion, Intensification,
            TransformationHeuristic>(config, benchmark_id);

  return 0;
}
