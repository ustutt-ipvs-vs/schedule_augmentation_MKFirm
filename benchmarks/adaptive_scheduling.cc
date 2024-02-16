#include "../src/optimization/tabu_search.h"

#include "setup.h"
#include <random>

#define SEED 1234

#define WIRELESS_DL_DMAX 5397000 // 5.397ms
#define WIRELESS_DL_DMIN 4833000 // 4.833ms

#define WIRELESS_UL_DMAX 5966000 // 5.397ms
#define WIRELESS_UL_DMIN 5348000 // 4.833ms

#define WIRELESS_TRAFFIC_DEADLINE 10000000 // 10ms
#define CROSS_TRAFFIC_DEADLINE 2000000     // 2ms

#define WIRELESS_TRAFFIC_JITTER 5000000 // 5ms
#define CROSS_TRAFFIC_JITTER 0

#define WIRELESS_TRAFFIC_PERIOD 10000000 // 10ms
#define CROSS_TRAFFIC_PERIOD 2000000     // 2ms

#define WIRELESS_FRAME_SIZE 100
#define WIRED_FRAME_SIZE 100

using namespace tsndgm;

int main(int argc, char **argv) {
  if (argc != 11) {
    std::cout << "Usage: ./adaptive_scheduling <talkers> <listeners> "
                 "<wireless_streams> <cross_traffic> "
                 "<timeout1> <timeout2> <timeout3> <timeout4> "
                 "<degradation_min> <degradation_max>"
              << std::endl;
    exit(0);
  }

  int talkers = pow(2, std::stoi(argv[1])),
      listeners = pow(2, std::stoi(argv[2])), streams = std::stoi(argv[3]),
      cross_traffic = std::stoi(argv[4]), timeout1 = std::stoi(argv[5]),
      timeout2 = std::stoi(argv[6]), timeout3 = std::stoi(argv[7]),
      timeout4 = std::stoi(argv[8]), degradation_min = std::stoi(argv[9]),
      degradation_max = std::stoi(argv[10]);

  // setup network topology
  std::vector<NetworkDeviceProperty> device_properties;
  std::vector<DataLink> data_links;
  int o = 0;
  for (int bridge = 0; bridge < talkers - 1; bridge++) {
    device_properties.push_back(
        NetworkDeviceProperty(bridge, 0, "Bt" + std::to_string(bridge)));
    if (bridge > 0) {
      data_links.push_back(
          DataLink(Edge(bridge, (bridge - 1 - ((bridge + 1) % 2)) / 2),
                   DataLinkProperty(wired)));
      data_links.push_back(
          DataLink(Edge((bridge - 1 - ((bridge + 1) % 2)) / 2, bridge),
                   DataLinkProperty(wired)));
    }
  }
  int to = talkers - 1; // talker offset
  for (int talker = 0; talker < talkers; talker++) {
    device_properties.push_back(
        NetworkDeviceProperty(to + talker, 0, "T" + std::to_string(talker)));
    data_links.push_back(DataLink(
        Edge(to + talker, (to + talker - 1 - ((to + talker + 1) % 2)) / 2),
        DataLinkProperty(wired)));
    data_links.push_back(DataLink(
        Edge((to + talker - 1 - ((to + talker + 1) % 2)) / 2, to + talker),
        DataLinkProperty(wired)));
  }
  int ho = to + talkers; // half offset
  for (int bridge = 0; bridge < listeners - 1; bridge++) {
    device_properties.push_back(
        NetworkDeviceProperty(ho + bridge, 0, "Bl" + std::to_string(bridge)));
    if (bridge > 0) {
      data_links.push_back(DataLink(
          Edge(ho + (bridge - 1 - ((bridge + 1) % 2)) / 2, ho + bridge),
          DataLinkProperty(wired)));
      data_links.push_back(DataLink(
          Edge(ho + bridge, ho + (bridge - 1 - ((bridge + 1) % 2)) / 2),
          DataLinkProperty(wired)));
    }
  }
  data_links.push_back(DataLink(Edge(0, ho), DataLinkProperty(wireless, true)));
  data_links.push_back(DataLink(Edge(ho, 0), DataLinkProperty(wireless, true)));
  int lo = ho + listeners - 1; // listener offset
  for (int listener = 0; listener < listeners; listener++) {
    device_properties.push_back(NetworkDeviceProperty(
        lo + listener, 0, "L" + std::to_string(listener)));
    data_links.push_back(DataLink(
        Edge(ho + (lo - ho + listener - 1 - ((lo - ho + listener + 1) % 2)) / 2,
             lo + listener),
        DataLinkProperty(wired)));
    data_links.push_back(DataLink(
        Edge(lo + listener,
             ho +
                 (lo - ho + listener - 1 - ((lo - ho + listener + 1) % 2)) / 2),
        DataLinkProperty(wired)));
  }

  auto network = make_shared<NetworkTopology>(device_properties, data_links);

  std::mt19937 gen;
  gen.seed(SEED);
  std::uniform_int_distribution dt(0, talkers - 1);
  std::uniform_int_distribution dl(0, listeners - 1);
  std::vector<MessageStream> message_streams;

  // wireless traffic from left to right
  for (int stream = 0; stream < streams; stream++) {
    PathRoute path, path1;
    RTIMap rti_map;

    int talker = dt(gen);
    int listener = dl(gen);

    for (int d = to + talker; d > 0; d = (d - 1 - ((d + 1) % 2)) / 2) {
      path.push_back(Edge(d, (d - 1 - ((d + 1) % 2)) / 2));
    }
    path.push_back(Edge(0, ho));
    rti_map[Edge(0, ho)] = RTI(WIRELESS_UL_DMAX, WIRELESS_UL_DMIN);
    for (int d = lo + listener; d > ho;
         d = ho + (d - ho - 1 - ((d - ho + 1) % 2)) / 2) {
      path1.push_front(Edge(ho + (d - ho - 1 - ((d - ho + 1) % 2)) / 2, d));
    }
    path.splice(path.end(), path1);

    std::shared_ptr<Route> route = make_shared<Route>(network, std::move(path));
    route->check();
    message_streams.push_back(MessageStream(
        network, route, WIRELESS_TRAFFIC_PERIOD, WIRELESS_FRAME_SIZE,
        WIRELESS_TRAFFIC_PERIOD, rti_map, 0, WIRELESS_TRAFFIC_JITTER));
  }

  // wireless traffic from right to left
  for (int stream = 0; stream < streams; stream++) {
    PathRoute path, path1;
    RTIMap rti_map;

    int talker = dl(gen);
    int listener = dt(gen);

    for (int d = lo + talker; d > ho;
         d = ho + (d - ho - 1 - ((d - ho + 1) % 2)) / 2) {
      path.push_back(Edge(d, ho + (d - ho - 1 - ((d - ho + 1) % 2)) / 2));
    }
    path.push_back(Edge(ho, 0));
    rti_map[Edge(ho, 0)] = RTI(WIRELESS_DL_DMAX, WIRELESS_DL_DMIN);
    for (int d = to + listener; d > 0; d = (d - 1 - ((d + 1) % 2)) / 2) {
      path1.push_front(Edge((d - 1 - ((d + 1) % 2)) / 2, d));
    }
    path.splice(path.end(), path1);

    std::shared_ptr<Route> route = make_shared<Route>(network, std::move(path));
    route->check();
    message_streams.push_back(MessageStream(
        network, route, WIRELESS_TRAFFIC_PERIOD, WIRELESS_FRAME_SIZE,
        WIRELESS_TRAFFIC_PERIOD, rti_map, 0, WIRELESS_TRAFFIC_JITTER));
  }

  // cross traffic for left half
  std::uniform_int_distribution dt1(0, talkers - 2);
  for (int stream = 0; stream < cross_traffic; stream++) {
    PathRoute path, path1;

    int talker = dt(gen);
    int listener = dt1(gen);
    if (listener >= talker)
      listener++;

    int dt = to + talker;
    int dl = to + listener;
    do {
      path.push_back(Edge(dt, (dt - 1 - ((dt + 1) % 2)) / 2));
      path1.push_front(Edge((dl - 1 - ((dl + 1) % 2)) / 2, dl));

      dt = (dt - 1 - ((dt + 1) % 2)) / 2;
      dl = (dl - 1 - ((dl + 1) % 2)) / 2;
    } while (dt != dl);

    path.splice(path.end(), path1);

    std::shared_ptr<Route> route = make_shared<Route>(network, std::move(path));
    route->check();
    for (int i = 0; i < WIRELESS_TRAFFIC_PERIOD / CROSS_TRAFFIC_PERIOD; i++) {
      message_streams.push_back(
          MessageStream(network, route, CROSS_TRAFFIC_PERIOD, WIRED_FRAME_SIZE,
                        i * CROSS_TRAFFIC_PERIOD + CROSS_TRAFFIC_DEADLINE, {},
                        i * CROSS_TRAFFIC_PERIOD, 0));
    }
  }

  // cross traffic for right half
  std::uniform_int_distribution dl1(0, listeners - 2);
  for (int stream = 0; stream < cross_traffic; stream++) {
    PathRoute path, path1;

    int talker = dl(gen);
    int listener = dl1(gen);
    if (listener >= talker)
      listener++;

    int dt = lo + talker;
    int dl = lo + listener;
    do {
      path.push_back(Edge(dt, ho + (dt - ho - 1 - ((dt - ho + 1) % 2)) / 2));
      path1.push_front(Edge(ho + (dl - ho - 1 - ((dl - ho + 1) % 2)) / 2, dl));

      dt = ho + (dt - ho - 1 - ((dt - ho + 1) % 2)) / 2;
      dl = ho + (dl - ho - 1 - ((dl - ho + 1) % 2)) / 2;
    } while (dt != dl);

    path.splice(path.end(), path1);

    std::shared_ptr<Route> route = make_shared<Route>(network, std::move(path));
    route->check();
    for (int i = 0; i < WIRELESS_TRAFFIC_PERIOD / CROSS_TRAFFIC_PERIOD; i++) {
      message_streams.push_back(
          MessageStream(network, route, CROSS_TRAFFIC_PERIOD, WIRED_FRAME_SIZE,
                        i * CROSS_TRAFFIC_PERIOD + CROSS_TRAFFIC_DEADLINE, {},
                        i * CROSS_TRAFFIC_PERIOD, CROSS_TRAFFIC_JITTER));
    }
  }

  TabuSearch tabu_search(network, message_streams);

  auto objective = CriticalPath::Objective::fixed_lateness;
  auto bound = CriticalPath::get_termination_bound(objective);

  TabuSearchConfig config{
      objective,
      TerminationConfig(timeout1, bound),
      IntensificationConfig(10, 200),
      DiversificationConfig(10),
      CompressionConfig(true, TerminationConfig(timeout2, bound),
                        IntensificationConfig(10, 100)),
  };

  using InitialHeuristic =
      CombinedInitial<RandomInitial, EffectiveReleaseInitial>;
  using TerminationCriterion = TimeoutTerminationCriterion;
  using Intensification = StrictAdmissionIntensification<
      DifferentialTerminationCriterion,
      ReducedSelectionCriticalBlockNeighborhood<1>>;
  using TransformationHeuristic =
      RandomCriticalPathTransformation<ConstantThenSlowTemperature>;

  if (timeout1 != 0 || timeout2 != 0) {
    tabu_search.run<InitialHeuristic, TerminationCriterion, Intensification,
                    TransformationHeuristic>(config);
    tabu_search.dgm.print_critical_path(config.type);

    std::cout << "\nUPDATING RTIs\n" << std::endl;
  }
  tabu_search.reset_timer();

  // update rtis of wireless streams
  std::map<MessageStreamHandle, RTIMap> rti_updates;
  for (int stream = 0; stream < streams; stream++) {
    rti_updates[stream] = {
        {Edge(0, ho), RTI(WIRELESS_UL_DMAX + degradation_max,
                          WIRELESS_UL_DMIN + degradation_min)}};
  }
  for (int stream = streams; stream < 2 * streams; stream++) {
    rti_updates[stream] = {
        {Edge(ho, 0), RTI(WIRELESS_DL_DMAX + degradation_max,
                          WIRELESS_DL_DMIN + degradation_min)}};
  }

  objective = CriticalPath::Objective::fixed_tardiness;
  bound = CriticalPath::get_termination_bound(objective);
  TabuSearchConfig config1{

      objective,
      TerminationConfig(timeout3, bound),
      IntensificationConfig(10, 200),
      DiversificationConfig(10),
      CompressionConfig(true, TerminationConfig(timeout4, bound),
                        IntensificationConfig(10, 10)),
  };
  tabu_search.run<InitialHeuristic, TerminationCriterion, Intensification,
                  TransformationHeuristic>(config1, rti_updates);
  tabu_search.dgm.print_critical_path(config1.type);

  return 0;
}
