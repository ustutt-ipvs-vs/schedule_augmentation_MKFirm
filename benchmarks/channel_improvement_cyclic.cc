#include "../src/optimization/tabu_search.h"

#include "setup.h"
#include <random>

#define SEED 1234

#define WIRELESS_DL_DMAX 5397000 // 5.397ms
#define WIRELESS_DL_DMIN 4833000 // 4.833ms

#define WIRELESS_UL_DMAX 5966000 // 5.397ms
#define WIRELESS_UL_DMIN 5348000 // 4.833ms

#define WIRELESS_TRAFFIC_DEADLINE 10000000 // 10ms
#define CROSS_TRAFFIC_DEADLINE 200000      // 200us

#define WIRELESS_TRAFFIC_JITTER 5000000 // 5ms
#define CROSS_TRAFFIC_JITTER 0

#define WIRELESS_TRAFFIC_PERIOD 10000000 // 10ms
#define CROSS_TRAFFIC_PERIOD 2000000     // 2ms

#define WIRELESS_FRAME_SIZE 100
#define WIRED_FRAME_SIZE 100

using namespace tsndgm;

int main(int argc, char **argv) {
  if (argc != 8) {
    std::cout << "Usage: ./adaptive_scheduling <talkers> <listeners> "
                 "<wireless_streams> <cross_traffic> <timeout>"
                 "<improvement_max> <improvement_step>"
              << std::endl;
    exit(0);
  }

  int talkers = pow(2, std::stoi(argv[1])),
      listeners = pow(2, std::stoi(argv[2])), streams = std::stoi(argv[3]),
      cross_traffic = std::stoi(argv[4]), timeout = std::stoi(argv[5]),
      improvement_max = std::stoi(argv[6]),
      improvement_step = std::stoi(argv[7]);

  // setup network topology
  std::vector<NetworkDeviceProperty> device_properties;
  std::vector<DataLink> data_links;
  int o = 0;
  for (int bridge = 0; bridge < talkers - 1; bridge++) {
    device_properties.push_back(
        NetworkDeviceProperty(bridge, 1000, "Bt" + std::to_string(bridge)));
    if (bridge > 0) {
      data_links.push_back(
          DataLink(Edge(bridge, (bridge - 1 - ((bridge + 1) % 2)) / 2),
                   DataLinkProperty(wired, 12500000, 50)));
      data_links.push_back(
          DataLink(Edge((bridge - 1 - ((bridge + 1) % 2)) / 2, bridge),
                   DataLinkProperty(wired, 12500000, 50)));
    }
  }
  int to = talkers - 1; // talker offset
  for (int talker = 0; talker < talkers; talker++) {
    device_properties.push_back(
        NetworkDeviceProperty(to + talker, 1000, "T" + std::to_string(talker)));
    data_links.push_back(DataLink(
        Edge(to + talker, (to + talker - 1 - ((to + talker + 1) % 2)) / 2),
        DataLinkProperty(wired, 12500000, 50)));
    data_links.push_back(DataLink(
        Edge((to + talker - 1 - ((to + talker + 1) % 2)) / 2, to + talker),
        DataLinkProperty(wired, 12500000, 50)));
  }
  int ho = to + talkers; // half offset
  for (int bridge = 0; bridge < listeners - 1; bridge++) {
    device_properties.push_back(NetworkDeviceProperty(
        ho + bridge, 1000, "Bl" + std::to_string(bridge)));
    if (bridge > 0) {
      data_links.push_back(DataLink(
          Edge(ho + (bridge - 1 - ((bridge + 1) % 2)) / 2, ho + bridge),
          DataLinkProperty(wired, 12500000, 50)));
      data_links.push_back(DataLink(
          Edge(ho + bridge, ho + (bridge - 1 - ((bridge + 1) % 2)) / 2),
          DataLinkProperty(wired, 12500000, 50)));
    }
  }
  data_links.push_back(DataLink(Edge(0, ho), DataLinkProperty(wireless, true)));
  data_links.push_back(DataLink(Edge(ho, 0), DataLinkProperty(wireless, true)));
  int lo = ho + listeners - 1; // listener offset
  for (int listener = 0; listener < listeners; listener++) {
    device_properties.push_back(NetworkDeviceProperty(
        lo + listener, 1000, "L" + std::to_string(listener)));
    data_links.push_back(DataLink(
        Edge(ho + (lo - ho + listener - 1 - ((lo - ho + listener + 1) % 2)) / 2,
             lo + listener),
        DataLinkProperty(wired, 12500000, 50)));
    data_links.push_back(DataLink(
        Edge(lo + listener,
             ho +
                 (lo - ho + listener - 1 - ((lo - ho + listener + 1) % 2)) / 2),
        DataLinkProperty(wired, 12500000, 50)));
  }

  auto network = make_shared<NetworkTopology>(device_properties, data_links);

  std::mt19937 gen;
  gen.seed(SEED);
  std::uniform_int_distribution dt(0, talkers - 1);
  std::uniform_int_distribution dl(0, listeners - 1);
  std::vector<MessageStream> message_streams;

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
                        CROSS_TRAFFIC_DEADLINE, {}, i * CROSS_TRAFFIC_PERIOD,
                        CROSS_TRAFFIC_JITTER, "CT_L"));
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
                        CROSS_TRAFFIC_DEADLINE, {}, i * CROSS_TRAFFIC_PERIOD,
                        CROSS_TRAFFIC_JITTER, "CT_R"));
    }
  }
  size_t offset = message_streams.size();

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
        WIRELESS_TRAFFIC_PERIOD, rti_map, 0, WIRELESS_TRAFFIC_JITTER, "W_LR"));
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
        WIRELESS_TRAFFIC_PERIOD, rti_map, 0, WIRELESS_TRAFFIC_JITTER, "W_RL"));
  }

  TabuSearch tabu_search(network, message_streams);
  if (tabu_search.com.rank != 0)
    std::cout.setstate(std::ios::failbit);

  auto objective = CriticalPath::Objective::dynamic_tardiness;
  auto bound = -145700;

  TabuSearchConfig config{
      objective,
      TerminationConfig(60, bound),
      IntensificationConfig(10, 500),
      DiversificationConfig(10, 10),
      CompressionConfig(true, TerminationConfig(timeout, bound),
                        IntensificationConfig(10, 50)),
  };

  using InitialHeuristic = EffectiveReleaseInitial;
  using TerminationCriterion = TimeoutTerminationCriterion;
  using Intensification = StrictAdmissionIntensification<
      DifferentialTerminationCriterion,
      ReducedSelectionCriticalBlockNeighborhood<1>>;
  using TransformationHeuristic =
      RandomCriticalPathTransformation<ConstantThenSlowTemperature>;

  tabu_search.run<InitialHeuristic, TerminationCriterion, Intensification,
                  TransformationHeuristic>(config);
  tabu_search.dgm.print_critical_path(objective);
  auto tabu_search1 = tabu_search;

  objective = CriticalPath::Objective::dynamic_lateness;
  if (tabu_search.dgm.critical_path(objective).objective > 0) {
    std::cout << "\noptimum not found; exiting...\n" << std::endl;
    return 0;
  }

  std::cout << "\nGRACEFUL IMPROVEMENT\n" << std::endl;

  // update rtis of wireless streams
  for (int improvement = 0; improvement <= improvement_max;
       improvement += improvement_step) {
    int improvement_lower[] = {-improvement, 0, improvement};
    for (int dl : improvement_lower) {
      if (WIRELESS_UL_DMAX - improvement <= WIRELESS_UL_DMIN - dl ||
          WIRELESS_DL_DMAX - improvement <= WIRELESS_DL_DMIN - dl ||
          WIRELESS_DL_DMIN < 0)
        continue;

      auto tabu_search_gd = tabu_search;
      ShuffleGraphProperty &prop =
          tabu_search_gd.dgm.shuffle_graph[boost::graph_bundle];
      std::map<MessageStreamHandle, RTIMap> rti_updates;

      for (MessageStreamHandle ms = 0; ms < prop.streams.size(); ms++) {
        if (prop.streams[ms].name.find("W_LR") != std::string::npos) {
          rti_updates[ms] = {{Edge(0, ho), RTI(WIRELESS_UL_DMAX - improvement,
                                               WIRELESS_UL_DMIN - dl)}};
        } else if (prop.streams[ms].name.find("W_RL") != std::string::npos) {
          rti_updates[ms] = {{Edge(ho, 0), RTI(WIRELESS_DL_DMAX - improvement,
                                               WIRELESS_DL_DMIN - dl)}};
        }
      }

      tabu_search_gd.update_rti(rti_updates, objective);
      std::cout << dl << ", " << WIRELESS_UL_DMIN - dl << ", "
                << WIRELESS_UL_DMAX - improvement << ", "
                << tabu_search_gd.dgm.critical_path(objective).objective
                << std::endl;

      Delay max_wl_lateness = std::numeric_limits<Delay>::min();
      for (MessageStreamHandle ms = 0; ms < prop.streams.size(); ms++) {
        if (prop.streams[ms].name.find("W_") != std::string::npos) {
          const std::list<Edge> &listeners =
              prop.streams[ms].route->get_listeners();

          for (Edge listener : listeners) {
            auto v_listener = prop.operation_to_vertex[{listener, ms}];
            max_wl_lateness =
                std::max(max_wl_lateness,
                         tabu_search_gd.dgm.crit_path.get_dynamic_lateness(
                             ms, listener));
          }
        }
      }
      std::cout << max_wl_lateness << std::endl;
    }
  }

  return 0;

  std::cout << "\nADAPTIVE\n" << std::endl;

  // update rtis of wireless streams
  for (int improvement = 0; improvement <= improvement_max;
       improvement += improvement_step) {
    int improvement_lower[] = {-improvement, 0, improvement};
    for (int dl : improvement_lower) {
      if (WIRELESS_UL_DMAX - improvement <= WIRELESS_UL_DMIN - dl ||
          WIRELESS_DL_DMAX - improvement <= WIRELESS_DL_DMIN - dl)
        continue;

      ShuffleGraphProperty &prop =
          tabu_search.dgm.shuffle_graph[boost::graph_bundle];
      std::map<MessageStreamHandle, RTIMap> rti_updates;

      for (MessageStreamHandle ms = 0; ms < prop.streams.size(); ms++) {
        if (prop.streams[ms].name.find("W_LR") != std::string::npos) {
          rti_updates[ms] = {{Edge(0, ho), RTI(WIRELESS_UL_DMAX - improvement,
                                               WIRELESS_UL_DMIN - dl)}};
        } else if (prop.streams[ms].name.find("W_RL") != std::string::npos) {
          rti_updates[ms] = {{Edge(ho, 0), RTI(WIRELESS_DL_DMAX - improvement,
                                               WIRELESS_DL_DMIN - dl)}};
        }
      }

      auto tabu_search_adaptive = tabu_search1;
      TabuSearchConfig config1{
          objective,
          TerminationConfig(2, bound),
          IntensificationConfig(10, 500),
          DiversificationConfig(10, 10),
          CompressionConfig(true, TerminationConfig(8, bound),
                            IntensificationConfig(10, 50)),
      };

      tabu_search_adaptive.com.sync();
      tabu_search_adaptive.reset_timer();
      tabu_search_adaptive.run<InitialHeuristic, TerminationCriterion,
                               Intensification, TransformationHeuristic>(
          config1, rti_updates);

      std::cout << dl << ", " << WIRELESS_UL_DMIN - dl << ", "
                << WIRELESS_UL_DMAX - improvement << ", "
                << tabu_search_adaptive.dgm.critical_path(objective).objective
                << std::endl;

      Delay max_wl_lateness = std::numeric_limits<Delay>::min();
      for (MessageStreamHandle ms = 0; ms < prop.streams.size(); ms++) {
        if (prop.streams[ms].name.find("W_") != std::string::npos) {
          const std::list<Edge> &listeners =
              prop.streams[ms].route->get_listeners();

          for (Edge listener : listeners) {
            auto v_listener = prop.operation_to_vertex[{listener, ms}];
            max_wl_lateness = std::max(
                max_wl_lateness,
                tabu_search_adaptive.dgm.crit_path.get_dynamic_lateness(
                    ms, listener));
          }
        }
      }
      std::cout << max_wl_lateness << std::endl;
    }
  }

  return 0;
}
