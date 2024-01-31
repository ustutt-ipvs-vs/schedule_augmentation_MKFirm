#include "../src/optimization/tabu_search.h"

#include "setup.h"

#define WIRELESS_DMAX 400
#define WIRELESS_DMIN 50
#define FRAME_SIZE 20

using namespace tsndgm;

int main(int argc, char **argv) {
  if (argc != 5) {
    std::cout << "Usage: ./dumbbell <talkers> <bridges> <listeners> <streams>"
              << std::endl;
    exit(0);
  }

  int talkers = stoi(argv[1]), bridges = stoi(argv[2]),
      listeners = stoi(argv[3]), streams = stoi(argv[4]);

  // setup network topology
  vector<NetworkDeviceProperty> device_properties;
  vector<DataLink> data_links;
  for (int bridge = 0; bridge < bridges; bridge++) {
    device_properties.push_back(
        NetworkDeviceProperty(bridge, 0, "B" + std::to_string(bridge)));
    if (bridge > 0) {
      data_links.push_back(DataLink(Edge(bridge - 1, bridge),
                                    DataLinkProperty(DataLinkType(wired))));
    }
  }
  int id_offset = bridges;
  for (int talker = 0; talker < talkers; talker++) {
    device_properties.push_back(NetworkDeviceProperty(
        id_offset + talker, 0, "T" + std::to_string(talker)));
    data_links.push_back(DataLink(Edge(id_offset + talker, 0),
                                  DataLinkProperty(DataLinkType(wireless))));
  }
  id_offset += talkers;
  for (int listener = 0; listener < listeners; listener++) {
    device_properties.push_back(NetworkDeviceProperty(
        id_offset + listener, 0, "L" + std::to_string(listener)));
    data_links.push_back(DataLink(Edge(bridges - 1, id_offset + listener),
                                  DataLinkProperty(DataLinkType(wired))));
  }
  auto network = make_shared<NetworkTopology>(device_properties, data_links);

  // setup streams
  std::vector<MessageStream> message_streams;
  for (int stream = 0; stream < streams; stream++) {
    PathRoute path;
    RTIMap rti_map;

    int talker = bridges + (stream % talkers);
    int listener = bridges + talkers + (stream % listeners);
    path.push_back(Edge(talker, 0));
    rti_map[Edge(talker, 0)] = RTI(WIRELESS_DMAX, WIRELESS_DMIN);
    for (int bridge = 0; bridge < bridges - 1; bridge++)
      path.push_back(Edge(bridge, bridge + 1));
    path.push_back(Edge(bridges - 1, listener));

    shared_ptr<Route> route = make_shared<Route>(network, std::move(path));
    route->check();
    message_streams.push_back(
        MessageStream(network, route, 100, FRAME_SIZE, 100, rti_map, 0, 10000));
  }

  TabuSearch tabu_search(network, message_streams);

  TabuSearchConfig config{CriticalPath::Objective::makespan,
                          TerminationConfig(10),
                          IntensificationConfig(10, 5 * streams),
                          DiversificationConfig(5),
                          1,
                          true};

  using InitialHeuristic = RandomInitial;
  using TerminationCriterion = DifferentialTerminationCriterion;
  using Intensification = StrictAdmissionIntensification<
      DifferentialTerminationCriterion,
      ReducedSelectionCriticalBlockNeighborhood<>>;
  using TransformationHeuristic = NoTransformation;

  tabu_search.run<InitialHeuristic, TerminationCriterion, Intensification,
                  TransformationHeuristic>(config);

  tabu_search.dgm.print_critical_path(config.type);

  return 0;
}
