#include <gtest/gtest.h>
#include <iostream>
#include <random>
#include <vector>

#include "../src/dgm/critical_path.h"
#include "../src/dgm/dgm.h"
#include "../src/heuristics/initial.h"
#include "../src/optimization/neighborhood.h"
#include "../src/optimization/tabu_search.h"
#include "../src/optimization/termination.h"

using namespace std;
using namespace tsndgm;

class DGMTest : public testing::Test {
protected:
  void SetUp() override {
    string devices[] = {"T_1", "T_2", "B_1", "B_2", "B_3", "B_4", "L_1"};
    unique_id = 0;
    vector<NetworkDeviceProperty> device_properties;
    for (int i = 0; i < 7; i++)
      device_properties.push_back(UniqueNetworkDeviceProperty(0));
    vector<DataLink> data_links;
    data_links.push_back(
        make_pair(Edge(0, 2), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(2, 3), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(3, 4), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(4, 6), DataLinkProperty(DataLinkType(wired))));

    data_links.push_back(
        make_pair(Edge(2, 5), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(5, 4), DataLinkProperty(DataLinkType(wired))));

    data_links.push_back(
        make_pair(Edge(1, 4), DataLinkProperty(DataLinkType(wired))));

    network = make_shared<NetworkTopology>(device_properties, data_links);

    PathRoute path1 = {Edge(0, 2), Edge(2, 3), Edge(3, 4), Edge(4, 6)};
    shared_ptr<Route> route1 = make_shared<Route>(network, std::move(path1));
    route1->check();
    MessageStream s1(network, route1, 1000, 60, 100);
    MessageStream s2(network, route1, 1000, 20, 100);

    PathRoute path2 = {Edge(1, 4), Edge(4, 6)};
    shared_ptr<Route> route2 = make_shared<Route>(network, std::move(path2));
    route2->check();
    MessageStream s3(network, route2, 1000, 20, 1000);

    PathRoute path3 = {Edge(0, 2), Edge(2, 5), Edge(5, 4), Edge(4, 6)};
    shared_ptr<Route> route3 = make_shared<Route>(network, std::move(path3));
    route3->check();
    MessageStream s4(network, route3, 1000, 20, 1000);
    MessageStream s5(network, route3, 1000, 20, 1000);

    streams = {std::move(s1), std::move(s2), std::move(s3), std::move(s4),
               std::move(s5)};
  }

  shared_ptr<NetworkTopology> network;
  std::vector<MessageStream> streams;
};

class DGMTest1 : public testing::Test {
protected:
  void SetUp() override {
    string devices[] = {"T_1", "T_2", "B_1", "L_1"};
    vector<NetworkDeviceProperty> device_properties;
    unique_id = 0;
    for (int i = 0; i < 4; i++)
      device_properties.push_back(UniqueNetworkDeviceProperty(0));

    vector<DataLink> data_links;
    data_links.push_back(
        make_pair(Edge(0, 2), DataLinkProperty(DataLinkType(wireless))));
    data_links.push_back(
        make_pair(Edge(2, 3), DataLinkProperty(DataLinkType(wired))));

    data_links.push_back(
        make_pair(Edge(1, 2), DataLinkProperty(DataLinkType(wired))));

    network = make_shared<NetworkTopology>(device_properties, data_links);

    PathRoute path1 = {Edge(0, 2), Edge(2, 3)};
    shared_ptr<Route> route1 = make_shared<Route>(network, std::move(path1));
    route1->check();
    MessageStream s1(network, route1, 1000, 20, 100);
    s1.rti_map[Edge(0, 2)] = RTI(400, 50);

    PathRoute path2 = {Edge(1, 2), Edge(2, 3)};
    shared_ptr<Route> route2 = make_shared<Route>(network, std::move(path2));
    route2->check();
    MessageStream s2(network, route2, 1000, 20, 1000);
    s2.rti_map[Edge(1, 2)] = RTI(400, 50);
    MessageStream s3(network, route2, 1000, 20, 1000);
    s3.rti_map[Edge(1, 2)] = RTI(55, 50);

    streams = {std::move(s1), std::move(s2), std::move(s3)};
  }

  shared_ptr<NetworkTopology> network;
  std::vector<MessageStream> streams;
};

TEST_F(DGMTest, TabuSearchTardiness) {
  TabuSearch tabu_search(network, streams);
  typedef StrictAdmissionIntensification<DefaultTerminationCriterion,
                                         SelectionFullNeighborhood>
      IntensificationPhase;

  using InitialHeuristic = NoInitial;
  using TerminationCriterion = DefaultTerminationCriterion;
  using Intensification =
      StrictAdmissionIntensification<DefaultTerminationCriterion,
                                     SelectionFullNeighborhood>;
  using ExhaustiveSearch = Intensification;
  using TransformationHeuristic = NoTransformation;

  TabuSearch::Config config = {5,
                               CriticalPath::Objective::makespan,
                               IntensificationConfig(5, 50),
                               ExhaustiveSearchConfig(5, 20),
                               RelinkingConfig(IntensificationConfig(5, 10)),
                               3};

  tabu_search.run<InitialHeuristic, TerminationCriterion, Intensification,
                  ExhaustiveSearch, TransformationHeuristic>(config);
}
