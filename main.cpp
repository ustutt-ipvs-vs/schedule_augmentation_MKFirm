#include <memory>

#include "src/IO/inputLoader.h"
#include "src/IO/outputWriter.h"
#include "src/IO/programOptions.h"
#include "src/dgm/critical_path.h"
#include "src/dgm/dgm.h"
#include "src/network/topology.h"

auto main(const int argc, char *argv[]) -> int {
  /*
   * TODO:
   * - build dgm graph from schedule
   * - update dgm edge weights
   * - check dgm graph for feasibility
   *
   *  - generic Topological sort (depth first search from sink to source)
   *  - Longest Path
   *  - add opening/closing times
   *  - update opening and closing values
   *    * prolongation
   *    * deferrment
   *    * ...
   *
   * - create output
   */
  std::vector<std::string> arguments(argv + 1, argv + argc);
  const auto options = [&] {
    try {
      return io::ProgramOptions(arguments);
    } catch (const std::runtime_error &e) {
      std::cout << e.what();
      std::exit(1);
    }
  }();

  auto network = io::load_topology(options.getTopologyPath());
  network.print_topology();
  std::cout << "Loaded network topology!\n";

  auto streams = io::load_time_triggered_traffic(options.getTimeTriggeredStreamsPath());
  std::cout << "Loaded TT-streams!\n";

  const auto emergency_streams = io::load_emergency_traffic(options.getEmergencyStreams());
  std::cout << "Loaded ET-streams!\n";
  for (const tsndgm::EmergencyStream &es : emergency_streams) {
    std::cout << es.to_string() << "\n";
  }

  const std::vector<tsndgm::StreamSchedule> scheduled_streams = io::load_schedule(options.getSchedulePath());
  std::cout << "Loaded schedule!\n";
  for (const tsndgm::StreamSchedule &ss : scheduled_streams) {
    std::cout << ss.toString() << "\n";
  }

  io::set_routes(scheduled_streams, streams);
  network.update_all_edges_aggregated_et_usage(emergency_streams);

  std::cout << "Input loading and parsing done!\n";

  tsndgm::DisjunctiveGraphModel dgm(network, streams, scheduled_streams);
  tsndgm::critical_path::compute_longest_paths(dgm);
  dgm.print_operations();

  //auto path_result = collect_critical_path_result(dgm.transmission_graph, tsndgm::critical_path::makespan);
  //tsndgm::critical_path::print(dgm, path_result);

  if (not dgm.checkDeadlineViolations()) {
    io::write_output(options.getOutputPath(), network, dgm);
    std::cout << "Writing output done!\n";
  } else {
    std::cout << "No valid schedule found!\n";
  }
}
