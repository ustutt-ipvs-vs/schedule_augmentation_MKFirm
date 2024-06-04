#include "../src/heuristics/initial.h"
#include "../src/heuristics/transformation.h"
#include "../src/network/histogram.h"
#include "../src/optimization/tabu_search.h"

using namespace tsndgm;

int main(int argc, char **argv) {
  if (argc != 4) {
    std::cout << "Usage: ./adaptive_scheduling <in_network_file> "
                 "<in_stream_file> <out_tsn_config>"
              << std::endl;
    exit(0);
  }

  std::string network_file = argv[1], stream_file = argv[2],
              tsn_config_file = argv[3];

  auto network = make_shared<NetworkTopology>(network_file);
  auto streams = load_streams(network, stream_file);

  using InitialHeuristic = EffectiveReleaseInitial;
  using TerminationCriterion = TimeoutTerminationCriterion;
  using Intensification = StrictAdmissionIntensification<
      DifferentialTerminationCriterion,
      ReducedSelectionCriticalBlockNeighborhood<1>>;
  using TransformationHeuristic =
      RandomCriticalPathTransformation<ConstantThenSlowTemperature>;

  TabuSearch tabu_search(network, streams);
  if (tabu_search.com.rank != 0) {
    std::cout.setstate(std::ios::failbit);
  }

  auto objective = CriticalPath::Objective::weighted_dynamic_lateness;
  auto bound = CriticalPath::get_termination_bound(objective);

  TabuSearchConfig config{
      objective,
      TerminationConfig(60, bound),
      IntensificationConfig(10, 500),
      DiversificationConfig(10, 10),
      CompressionConfig(true, TerminationConfig(140, bound),
                        IntensificationConfig(10, 50), 30),
  };

  tabu_search.run<InitialHeuristic, TerminationCriterion, Intensification,
                  TransformationHeuristic>(config);

  if (tabu_search.com.rank == 0) {
    TSNConfiguration tsn_config = tabu_search.dgm.derive_tsn_configuration();
    tsn_config.dump(tsn_config_file);
  }
}
