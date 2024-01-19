#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <nlohmann/json.hpp>

#include "../src/heuristics/initial.h"
#include "../src/optimization/neighborhood.h"
#include "../src/optimization/tabu_search.h"
#include "../src/optimization/termination.h"
#include "setup.h"

using json = nlohmann::json;
using namespace std;
using namespace tsndgm;
namespace fs = std::filesystem;

template <class InitialHeuristic, class TerminationCriterion,
          class Intensification, class ExhaustiveSearch,
          class TransformationHeuristic>
void benchmark_instance(
    json &benchmark_data,
    const std::function<TabuSearch::Config(int, int)> &config) {

  JSPwithFIFOSetup jsp;
  jsp.setup(benchmark_data);

  auto c = config(jsp.machines, jsp.jobs);
  if (!benchmark_data["optimum"].is_null()) {
    c.termination_bound = benchmark_data["optimum"].template get<Delay>();
  } else {
    c.termination_bound =
        benchmark_data["bounds"]["lower"].template get<Delay>();
  }

  DisjunctiveGraphModel dgm(jsp.network, jsp.streams);
  // set_initial_solution(dgm, machine_to_datalink, machines, jobs);

  TabuSearch tabu_search(dgm);

  cout << benchmark_data["name"].template get<std::string>() << std::endl;

  tabu_search.run<InitialHeuristic, TerminationCriterion, Intensification,
                  ExhaustiveSearch, TransformationHeuristic>(c);

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
          class Intensification, class ExhaustiveSearch,
          class TransformationHeuristic>
void benchmark(const std::function<TabuSearch::Config(int, int)> &config,
               int benchmark_id) {
  std::ifstream f("../data/JSPLIB/instances.json");
  json data = json::parse(f);

  auto start = std::chrono::high_resolution_clock::now();
  benchmark_instance<InitialHeuristic, TerminationCriterion, Intensification,
                     ExhaustiveSearch, TransformationHeuristic>(
      data[benchmark_id], config);
  auto stop = std::chrono::high_resolution_clock::now();
  auto duration = duration_cast<std::chrono::seconds>(stop - start);
  cout << "Time: " << duration.count() << " s" << endl;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cout << "Usage: ./jsp [0-161]" << std::endl;
    exit(0);
  }

  auto config = [](int machines, int jobs) {
    return TabuSearch::Config{
        5,
        CriticalPath::Objective::makespan,
        IntensificationConfig(machines, 10 * machines * jobs),
        ExhaustiveSearchConfig(machines, 10 * machines * jobs),
        RelinkingConfig(IntensificationConfig(machines, 10 * machines * jobs)),
        5,
        5,
        true};
  };

  int benchmark_id = stoi(argv[1]);

  using InitialHeuristic = RandomInitial;
  using TerminationCriterion = DifferentialTerminationCriterion;
  using Intensification =
      TestStrictIntensification<DifferentialTerminationCriterion,
                                ReducedSelectionCriticalBlockNeighborhood>;
  using ExhaustiveSearch =
      TestStrictIntensification<DifferentialTerminationCriterion,
                                ReducedSelectionCriticalBlockNeighborhood>;
  using TransformationHeuristic = SlackTransformation;

  benchmark<InitialHeuristic, TerminationCriterion, Intensification,
            ExhaustiveSearch, TransformationHeuristic>(config, benchmark_id);

  return 0;
}
