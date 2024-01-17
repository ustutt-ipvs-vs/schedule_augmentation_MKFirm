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

using json = nlohmann::json;
using namespace std;
using namespace tsndgm;
namespace fs = std::filesystem;

void print(DisjunctiveGraphModel &dgm,
           std::map<int, Edge> &machine_to_datalink) {
  auto &prop = dgm.shuffle_graph[boost::graph_bundle];

  for (auto &[m, e] : machine_to_datalink) {
    vector<MessageStreamHandle> processing_order;
    for (MessageStreamHandle ms : prop.edge_to_streams[e]) {
      auto v = prop.operation_to_vertex[{e, ms}];
      size_t i;
      for (i = 0; i < processing_order.size(); i++) {
        auto u = prop.operation_to_vertex[{e, processing_order[i]}];
        auto e = dgm.edge(u, v);
        if (dgm.shuffle_graph[e].state() == OrientationState::blocked)
          break;
      }
      processing_order.insert(processing_order.begin() + i, ms);
    }

    std::cout << "M" << m << ": ";
    for (auto ms : processing_order)
      std::cout << ms << " ";
    std::cout << std::endl;
  }
}

void set_initial_solution(DisjunctiveGraphModel &dgm,
                          std::map<int, Edge> &machine_to_datalink,
                          int machines, int jobs) {
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  auto &prop = dgm.shuffle_graph[boost::graph_bundle];

  auto file = fs::path("../data/solution");
  if (!fs::exists(file))
    throw std::runtime_error("file does not exist: " + file.string());

  std::ifstream f(file);

  std::map<Edge, std::vector<V>> processing_order;
  for (int i = 0; i < machines; i++) {
    processing_order[machine_to_datalink[i]] = std::vector<V>(jobs);
    auto &machine_processing_order = processing_order[machine_to_datalink[i]];
    for (int j = 0; j < jobs; j++) {
      int job;
      f >> job;
      machine_processing_order[j] =
          prop.operation_to_vertex[{machine_to_datalink[i], job}];
    }
  }

  dgm.apply_processing_order(processing_order);

  dgm.commit_flips();
}

template <class InitialHeuristic, class TerminationCriterion,
          class Intensification, class ExhaustiveSearch,
          class TransformationHeuristic>
void benchmark_instance(
    json &benchmark_data,
    const std::function<TabuSearch::Config(int, int)> &config) {

  unique_id = 0;

  // read data from benchmark file
  auto file = fs::path("../data/JSPLIB/") /
              fs::path(benchmark_data["path"].template get<std::string>());
  if (!fs::exists(file))
    throw std::runtime_error("file does not exist: " + file.string());

  std::ifstream f(file);
  while (f.peek() == '#')
    f.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

  int jobs, machines;
  f >> jobs >> machines;
  if (jobs != benchmark_data["jobs"].template get<size_t>() ||
      machines != benchmark_data["machines"].template get<size_t>())
    throw std::runtime_error("instance description and actual data differs");

  int operations[jobs][2 * machines];
  for (int i = 0; i < jobs; i++)
    for (int j = 0; j < 2 * machines; j++)
      f >> operations[i][j];

  // convert JSP to TSN problem instance
  vector<NetworkDeviceProperty> device_properties;
  vector<DataLink> data_links;
  std::vector<MessageStream> streams;

  std::map<int, Edge> machine_to_datalink;
  for (int i = 0; i < machines; i++) {
    device_properties.push_back(
        NetworkDeviceProperty(2 * i, 0, "M" + std::to_string(i) + "1"));
    DeviceId id1 = device_properties.back().id;
    device_properties.push_back(
        NetworkDeviceProperty(2 * i + 1, 0, "M" + std::to_string(i) + "2"));
    DeviceId id2 = device_properties.back().id;

    machine_to_datalink[i] = Edge(id1, id2);
    data_links.push_back(
        DataLink(Edge(id1, id2), DataLinkProperty(DataLinkType(wireless))));
  }

  auto network = make_shared<NetworkTopology>(device_properties, data_links);

  for (int i = 0; i < jobs; i++) {
    PathRoute path;
    RTIMap rti_map;
    for (int j = 0; j < 2 * machines; j += 2) {
      Edge edge = machine_to_datalink[operations[i][j]];
      rti_map[edge] = RTI(operations[i][j + 1], operations[i][j + 1]);
      path.push_back(edge);
      // auto device = UniqueNetworkDeviceProperty(0);
      // network->add_device(device);
      // edge = Edge(edge.second, device.id);
      // network->add_data_link(
      //     DataLink(edge, DataLinkProperty(DataLinkType(wired))));
      // path.push_back(edge);
      if (j + 2 < 2 * machines) {
        Edge n_edge = machine_to_datalink[operations[i][j + 2]];
        network->add_data_link(DataLink(Edge(edge.second, n_edge.first),
                                        DataLinkProperty(DataLinkType(wired))));
        path.push_back(Edge(edge.second, n_edge.first));
      }
    }

    shared_ptr<Route> route = make_shared<Route>(network, std::move(path));
    route->check();
    streams.push_back(MessageStream(network, route, 100, 0, 100, rti_map));
  }

  auto c = config(machines, jobs);
  DisjunctiveGraphModel dgm(network, streams);
  // set_initial_solution(dgm, machine_to_datalink, machines, jobs);

  TabuSearch tabu_search(dgm);

  cout << benchmark_data["name"].template get<std::string>() << std::endl;

  if (!benchmark_data["optimum"].is_null()) {
    c.termination_bound = benchmark_data["optimum"].template get<Delay>();
  } else {
    c.termination_bound =
        benchmark_data["bounds"]["lower"].template get<Delay>();
  }

  tabu_search.run<InitialHeuristic, TerminationCriterion, Intensification,
                  ExhaustiveSearch, TransformationHeuristic>(c);

  if (!benchmark_data["optimum"].is_null()) {
    cout << "Known Optimal Solution: " << benchmark_data["optimum"]
         << std::endl;
  } else {
    cout << "Known Optimal Solution: [" << benchmark_data["bounds"]["lower"]
         << ", " << benchmark_data["bounds"]["upper"] << "]" << std::endl;
  }

  tabu_search.dgm.restore_commit(c.commit_index);
  print(dgm, machine_to_datalink);
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
        5};
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
