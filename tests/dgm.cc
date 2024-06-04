#include <gtest/gtest.h>
#include <iostream>
#include <random>
#include <vector>

#include "../src/dgm/complete_flip.h"
#include "../src/dgm/critical_path.h"
#include "../src/dgm/dgm.h"

using namespace std;
using namespace tsndgm;

class DGMTest : public testing::Test {
protected:
  void SetUp() override {
    unique_id = 0;
    string devices[] = {"T_1", "T_2", "B_1", "B_2", "B_3", "B_4", "L_1"};
    vector<NetworkDeviceProperty> device_properties;
    for (int i = 0; i < 7; i++)
      device_properties.push_back(NetworkDeviceProperty(i, 0, devices[i]));
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
    MessageStream s1(network, route1, 1000, 20, 100);
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
    string devices[] = {"T_1", "T_2", "T_3", "T_4", "B_1", "B_2", "B_3",
                        "B_4", "B_5", "B_6", "L_1", "L_2", "L_3", "L_4"};
    vector<NetworkDeviceProperty> device_properties;
    for (int i = 0; i < 14; i++)
      device_properties.push_back(NetworkDeviceProperty(i, 0, devices[i]));
    vector<DataLink> data_links;
    // T_1 -> B_1 -> B_2 -> B_3 -> L_1
    data_links.push_back(
        make_pair(Edge(0, 4), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(4, 5), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(5, 6), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(6, 10), DataLinkProperty(DataLinkType(wired))));
    // T_2 -> B_4 -> B_5 -> B_1 -> B_2 -> L_2
    data_links.push_back(
        make_pair(Edge(1, 7), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(7, 8), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(8, 4), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(5, 11), DataLinkProperty(DataLinkType(wired))));
    // T_3 -> B_4 -> B_5 -> B_6 -> L_3
    data_links.push_back(
        make_pair(Edge(2, 7), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(8, 9), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(9, 12), DataLinkProperty(DataLinkType(wired))));
    // T_4 -> B_2 -> B_3 -> B_5 -> B_6 -> L_4
    data_links.push_back(
        make_pair(Edge(3, 5), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(6, 8), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(9, 13), DataLinkProperty(DataLinkType(wired))));

    network = make_shared<NetworkTopology>(device_properties, data_links);

    PathRoute path1 = {Edge(0, 4), Edge(4, 5), Edge(5, 6), Edge(6, 10)};
    shared_ptr<Route> route1 = make_shared<Route>(network, std::move(path1));
    route1->check();
    MessageStream s1(network, route1, 1000, 20, 100);

    PathRoute path2 = {Edge(1, 7), Edge(7, 8), Edge(8, 4), Edge(4, 5),
                       Edge(5, 11)};
    shared_ptr<Route> route2 = make_shared<Route>(network, std::move(path2));
    route2->check();
    MessageStream s2(network, route2, 1000, 20, 100);

    PathRoute path3 = {Edge(2, 7), Edge(7, 8), Edge(8, 9), Edge(9, 12)};
    shared_ptr<Route> route3 = make_shared<Route>(network, std::move(path3));
    route3->check();
    MessageStream s3(network, route3, 1000, 20, 100);

    PathRoute path4 = {Edge(3, 5), Edge(5, 6), Edge(6, 8), Edge(8, 9),
                       Edge(9, 13)};
    shared_ptr<Route> route4 = make_shared<Route>(network, std::move(path4));
    route4->check();
    MessageStream s4(network, route4, 1000, 20, 100);

    streams = {std::move(s1), std::move(s2), std::move(s3), std::move(s4)};
  }

  shared_ptr<NetworkTopology> network;
  std::vector<MessageStream> streams;
};

class DGMTest2 : public testing::Test {
protected:
  void SetUp() override {
    string devices[] = {"T_1", "T_2", "T_3", "T_4", "B_1", "B_2", "B_3",
                        "B_4", "B_5", "B_6", "L_1", "L_2", "L_3", "L_4"};
    vector<NetworkDeviceProperty> device_properties;
    for (int i = 0; i < 14; i++)
      device_properties.push_back(NetworkDeviceProperty(i, 0, devices[i]));
    vector<DataLink> data_links;
    // T_1 -> B_1 -> B_2 -> B_3 -> L_1
    data_links.push_back(
        make_pair(Edge(0, 4), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(4, 5), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(5, 6), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(6, 10), DataLinkProperty(DataLinkType(wired))));
    // T_2 -> B_4 -> B_5 -> B_1 -> B_2 -> L_2
    data_links.push_back(
        make_pair(Edge(1, 7), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(7, 8), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(8, 4), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(5, 11), DataLinkProperty(DataLinkType(wired))));
    // T_3 -> B_4 -> B_5 -> B_6 -> L_3
    data_links.push_back(
        make_pair(Edge(2, 7), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(8, 9), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(9, 12), DataLinkProperty(DataLinkType(wired))));
    // T_4 -> B_2 -> B_3 -> B_4 -> B_5 -> L_4
    data_links.push_back(
        make_pair(Edge(3, 5), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(6, 7), DataLinkProperty(DataLinkType(wired))));
    data_links.push_back(
        make_pair(Edge(8, 13), DataLinkProperty(DataLinkType(wired))));

    network = make_shared<NetworkTopology>(device_properties, data_links);

    PathRoute path1 = {Edge(0, 4), Edge(4, 5), Edge(5, 6), Edge(6, 10)};
    shared_ptr<Route> route1 = make_shared<Route>(network, std::move(path1));
    route1->check();
    MessageStream s1(network, route1, 1000, 20, 100);

    PathRoute path2 = {Edge(1, 7), Edge(7, 8), Edge(8, 4), Edge(4, 5),
                       Edge(5, 11)};
    shared_ptr<Route> route2 = make_shared<Route>(network, std::move(path2));
    route2->check();
    MessageStream s2(network, route2, 1000, 20, 100);

    PathRoute path3 = {Edge(2, 7), Edge(7, 8), Edge(8, 9), Edge(9, 12)};
    shared_ptr<Route> route3 = make_shared<Route>(network, std::move(path3));
    route3->check();
    MessageStream s3(network, route3, 1000, 20, 100);

    PathRoute path4 = {Edge(3, 5), Edge(5, 6), Edge(6, 7), Edge(7, 8),
                       Edge(8, 13)};
    shared_ptr<Route> route4 = make_shared<Route>(network, std::move(path4));
    route4->check();
    MessageStream s4(network, route4, 1000, 20, 100);

    streams = {std::move(s1), std::move(s2), std::move(s3), std::move(s4)};
  }

  shared_ptr<NetworkTopology> network;
  std::vector<MessageStream> streams;
};

static void perform_operations(DisjunctiveGraphModel &dgm, int N, int SPLIT,
                               int SHUFFLE, int FLIP_RESTORE) {
  CriticalPath critical_path(dgm.transmission_graph);
  TransmissionGraphProperty &prop =
      boost::get_property(dgm.transmission_graph, boost::graph_bundle);

  for (int i = 1; i < N; i++) {
    std::cout << " " << i
              << "-------------------------------------------------------"
              << std::endl;
    if (i % SPLIT == 0) {
      std::cout << "TEST Split_All" << std::endl;
      dgm.split_all();
      assert_synchronicity(dgm.transmission_graph);
    } else if (i % SHUFFLE == 0) {
      vector<boost::graph_traits<transmission_graph_t>::edge_descriptor>
          eligible_edges;
      for (auto ed :
           boost::make_iterator_range(boost::edges(dgm.transmission_graph))) {
        if (dgm.transmission_graph[ed].edge_type == disjunctive &&
            dgm.transmission_graph[ed].state() == allowed) {
          eligible_edges.push_back(ed);
        }
      }
      if (eligible_edges.size() == 0)
        return;

      std::mt19937 generator(std::random_device{}());
      std::uniform_int_distribution<std::size_t> distribution(
          0, eligible_edges.size() - 1);
      auto ed = eligible_edges[distribution(generator)];

      std::cout << "TEST Shuffle: ";
      dgm.print(ed);
      std::cout << std::endl;

      try {
        dgm.complete_shuffle(ed);
      } catch (UnfixableCycleException &e) {
        std::cout << "WARNING: " << e.what() << std::endl;
      } catch (JitterBoundViolation &e) {
        std::cout << "WARNING: " << e.what() << std::endl;
      }
      dgm.print();
      assert_synchronicity(dgm.transmission_graph);

      for (auto v :
           boost::make_iterator_range(boost::vertices(dgm.transmission_graph))) {
        for (auto ms : dgm.transmission_graph[v].ms_handle) {
          ASSERT_TRUE(std::any_of(
              dgm.transmission_graph[v].JS.begin(), dgm.transmission_graph[v].JS.end(),
              [&](auto &nv) {
                return nv.v == 1 ||
                       std::find(dgm.transmission_graph[nv.v].ms_handle.begin(),
                                 dgm.transmission_graph[nv.v].ms_handle.end(),
                                 ms) != dgm.transmission_graph[nv.v].ms_handle.end();
              }));
          ASSERT_TRUE(std::any_of(
              dgm.transmission_graph[v].JP.begin(), dgm.transmission_graph[v].JP.end(),
              [&](auto &nv) {
                return nv.v == 0 ||
                       std::find(dgm.transmission_graph[nv.v].ms_handle.begin(),
                                 dgm.transmission_graph[nv.v].ms_handle.end(),
                                 ms) != dgm.transmission_graph[nv.v].ms_handle.end();
              }));
        }
      }

    } else if (i % FLIP_RESTORE == 0) {
      std::cout << "TEST Flip Restore: " << std::endl;
      dgm.restore_flips();
      assert_synchronicity(dgm.transmission_graph);
    } else {
      vector<boost::graph_traits<transmission_graph_t>::edge_descriptor>
          eligible_edges;
      for (auto ed :
           boost::make_iterator_range(boost::edges(dgm.transmission_graph))) {
        if (dgm.transmission_graph[ed].edge_type == disjunctive &&
            dgm.transmission_graph[ed].state() == allowed) {
          eligible_edges.push_back(ed);
        }
      }
      if (eligible_edges.size() == 0)
        return;

      std::mt19937 generator(std::random_device{}());
      std::uniform_int_distribution<std::size_t> distribution(
          0, eligible_edges.size() - 1);
      auto ed = eligible_edges[distribution(generator)];

      std::cout << "TEST Complete Flip: ";
      dgm.print(ed);
      std::cout << std::endl;

      try {
        dgm.complete_flip(ed);
      } catch (FlipGraphException &e) {
        std::cout << "WARNING: " << e.what() << ": " << e.required_shuffle
                  << std::endl;
      }
      dgm.print();
      assert_synchronicity(dgm.transmission_graph);
    }
  }
}

// The following snipped can be used to recreate initial situations
// dgm.complete_flip(dgm.edge(19, 9));
// dgm.complete_shuffle(dgm.edge(9, 19));

// std::map<Edge,
//          std::vector<boost::graph_traits<transmission_graph_t>::vertex_descriptor>>
//     processing_order = {{Edge(0, 2), {2, 6}},   {Edge(2, 3), {3, 7}},
//                         {Edge(3, 4), {4, 8}},   {Edge(2, 5), {13, 17}},
//                         {Edge(5, 4), {14, 18}}, {Edge(4, 6), {5, 11, 15,
//                         9}}};
// dgm.apply_processing_order(processing_order);
// dgm.print();
// dgm.complete_shuffle(dgm.edge(2, 6));
// dgm.print();

TEST_F(DGMTest, DGMOperation) {
  DisjunctiveGraphModel dgm(network, streams);

  perform_operations(dgm, 10000, 50, 15, 5);
}

TEST_F(DGMTest1, DGMOperation) {
  DisjunctiveGraphModel dgm(network, streams);

  perform_operations(dgm, 10000, 100, 25, 5);
}

TEST_F(DGMTest2, DGMOperation) {
  DisjunctiveGraphModel dgm(network, streams);

  perform_operations(dgm, 10000, 100, 20, 5);
}
