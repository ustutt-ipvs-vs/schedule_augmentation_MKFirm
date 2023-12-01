#include <gtest/gtest.h>
#include <iostream>
#include <random>
#include <vector>

#include "../src/dgm/critical_path.h"
#include "../src/dgm/dgm.h"

using namespace std;
using namespace tsndgm;

class DGMTest : public testing::Test {
protected:
  void SetUp() override {
    string devices[] = {"T_1", "T_2", "B_1", "B_2", "B_3", "B_4", "L_1"};
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

/* Perform SplitAll / LazyShuffle / CompleteFlip operations on the shuffle
 * graph and verify the following postconditions:
 *
 * SplitAll:
 *  - no message streams are shuffled
 *  - operation_to_vertex mapping is correctly reset
 *
 * LazyShuffle:
 *  - edge equivalence classes are correctly updated
 *  - operation_to_vertex mapping is correctly updated
 *  - shuffle graph is correctly updated, i.e.,
 *    - target(e) and source(e) are shuffled now
 *    - fifo edges are correctly remapped / removed
 *
 * CompleteFlip:
 *  - e is blocked now
 *  - rev_e is allowed now
 */
TEST_F(DGMTest, CompleteFlip) {
  DisjunctiveGraphModel dgm(network, streams);
  CriticalPath critical_path(dgm.shuffle_graph);
  ShuffleGraphProperty &prop =
      boost::get_property(dgm.shuffle_graph, boost::graph_bundle);

  for (int i = 0; i < 1000; i++) {
    std::cout << " " << i
              << "-------------------------------------------------------"
              << std::endl;
    if (i % 50 == 0) {
      std::cout << "TEST Split_All" << std::endl;
      dgm.split_all();

      critical_path.compute_longest_paths();

      // check that no message streams are shuffled
      for (auto vd :
           boost::make_iterator_range(boost::vertices(dgm.shuffle_graph))) {
        if (vd != prop.src && vd != prop.sink) {
          ASSERT_EQ(dgm.shuffle_graph[vd].ms_handle.size(), 1);
          ASSERT_EQ(prop.operation_to_vertex[Operation(
                        dgm.shuffle_graph[vd].edge,
                        dgm.shuffle_graph[vd].ms_handle.front())],
                    vd);
        }
      }
    } else if (i % 20 == 0) {
      vector<boost::graph_traits<shuffle_graph_t>::edge_descriptor>
          eligible_edges;
      for (auto ed :
           boost::make_iterator_range(boost::edges(dgm.shuffle_graph))) {
        if (dgm.shuffle_graph[ed].edge_type == disjunctive &&
            dgm.shuffle_graph[ed].state() == boost::allowed) {
          eligible_edges.push_back(ed);
        }
      }
      if (eligible_edges.size() == 0)
        return;

      std::mt19937 generator(std::random_device{}());
      std::uniform_int_distribution<std::size_t> distribution(
          0, eligible_edges.size() - 1);
      auto ed = eligible_edges[distribution(generator)];
      std::cout << "TEST Lazy Shuffle: ";
      tsndgm::print(dgm.shuffle_graph,
                    boost::get_property(dgm.shuffle_graph, boost::graph_bundle),
                    ed);
      std::cout << std::endl;

      std::list<MessageStreamHandle> old_list =
          dgm.shuffle_graph[target(ed, dgm.shuffle_graph)].ms_handle;

      dgm.lazy_shuffle(ed);

      std::list<MessageStreamHandle> &new_list =
          dgm.shuffle_graph[source(ed, dgm.shuffle_graph)].ms_handle;

      // check that source(ed) and target(ed) are shuffled now
      for (auto handle : old_list)
        ASSERT_NE(std::find(new_list.begin(), new_list.end(), handle),
                  new_list.end());

      // verify equivalence classes and shuffle graph
      for (auto e :
           boost::make_iterator_range(boost::edges(dgm.shuffle_graph))) {
        if (dgm.shuffle_graph[e].edge_type != disjunctive)
          continue;

        auto u = source(e, dgm.shuffle_graph), v = target(e, dgm.shuffle_graph);

        std::list<MessageStreamHandle>::const_iterator ms_it, ms_it1;
        std::list<const TreeRouteHop *>::iterator hop_it, hop_it1;
        for (ms_it = dgm.shuffle_graph[v].ms_handle.begin(),
            hop_it = dgm.shuffle_graph[v].hop.begin();
             ms_it != dgm.shuffle_graph[v].ms_handle.end() &&
             hop_it != dgm.shuffle_graph[v].hop.end();
             ++ms_it, ++hop_it) {

          // verify operation_to_vertex mapping
          ASSERT_EQ(prop.operation_to_vertex[Operation(
                        dgm.shuffle_graph[v].edge, *ms_it)],
                    v);

          if ((*hop_it)->parent->is_root())
            continue;

          // check if either fifo edge or predecessor exists
          bool valid = false;

          // fifo edge ~ e
          auto fifo_edge = boost::edge(u,
                                       prop.operation_to_vertex[Operation(
                                           (*hop_it)->parent->edge, *ms_it)],
                                       dgm.shuffle_graph);
          if (fifo_edge.second) {
            // std::cout << "Verify FIFO Edge: ";
            // tsndgm::print(dgm.shuffle_graph, prop, e);
            // std::cout << " ~ ";
            // tsndgm::print(dgm.shuffle_graph, prop, fifo_edge.first);
            // std::cout << std::endl;
            valid = true;
            ASSERT_EQ(dgm.shuffle_graph[e].relates_to(
                          dgm.shuffle_graph[fifo_edge.first]),
                      true);
          }

          // predecessor ~ e
          for (ms_it1 = dgm.shuffle_graph[u].ms_handle.begin(),
              hop_it1 = dgm.shuffle_graph[u].hop.begin();
               ms_it1 != dgm.shuffle_graph[u].ms_handle.end() &&
               hop_it1 != dgm.shuffle_graph[u].hop.end();
               ++ms_it1, ++hop_it1) {
            if ((*hop_it1)->parent->is_root())
              continue;

            auto pred_edge =
                boost::edge(prop.operation_to_vertex[Operation(
                                (*hop_it1)->parent->edge, *ms_it1)],
                            prop.operation_to_vertex[Operation(
                                (*hop_it)->parent->edge, *ms_it)],
                            dgm.shuffle_graph);
            if (pred_edge.second) {
              // std::cout << "Verify PRED Edge: ";
              // tsndgm::print(dgm.shuffle_graph, prop, e);
              // std::cout << " ~ ";
              // tsndgm::print(dgm.shuffle_graph, prop, pred_edge.first);
              // std::cout << std::endl;
              valid = true;
              ASSERT_EQ(dgm.shuffle_graph[e].relates_to(
                            dgm.shuffle_graph[pred_edge.first]),
                        true);
            }
          }

          ASSERT_EQ(valid, true);
        }
      }

      // throws runtime_error if shuffle_graph is acyclic
      critical_path.compute_longest_paths();
    } else {
      vector<boost::graph_traits<shuffle_graph_t>::edge_descriptor>
          eligible_edges;
      for (auto ed :
           boost::make_iterator_range(boost::edges(dgm.shuffle_graph))) {
        if (dgm.shuffle_graph[ed].edge_type == disjunctive &&
            dgm.shuffle_graph[ed].state() == boost::allowed) {
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
      tsndgm::print(dgm.shuffle_graph,
                    boost::get_property(dgm.shuffle_graph, boost::graph_bundle),
                    ed);
      std::cout << std::endl;

      // ed should be blocked now, and rev_ed allowed
      dgm.complete_flip(ed);
      ASSERT_EQ(dgm.shuffle_graph[ed].state(), boost::blocked);
      auto rev_ed =
          boost::edge(boost::target(ed, dgm.shuffle_graph),
                      boost::source(ed, dgm.shuffle_graph), dgm.shuffle_graph)
              .first;
      ASSERT_EQ(dgm.shuffle_graph[rev_ed].state(), boost::allowed);

      // throws runtime_error if shuffle_graph is acyclic
      critical_path.compute_longest_paths();
    }
  }
}
