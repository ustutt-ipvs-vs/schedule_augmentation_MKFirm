#include "dgm/critical_path.h"
#include <IO/inputLoader.h>
#include <dgm/dgm.h>
#include <gtest/gtest.h>

// fixture class
class CalculationOutputTestSequential : public testing::Test {
public:
  static tsndgm::NetworkTopology network;
  static std::unordered_map<tsndgm::StreamID, tsndgm::MessageStream> streams;
  static std::vector<tsndgm::StreamSchedule> scheduled_streams;
  static std::vector<tsndgm::EmergencyStream> emergency_streams;

  typedef boost::graph_traits<tsndgm::transmission_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<tsndgm::transmission_graph_t>::edge_descriptor E;

protected:
  CalculationOutputTestSequential() : dgm(network, streams, scheduled_streams) {
    // Code in this constructor will be executed before *each* test
    tsndgm::critical_path::compute_longest_paths(dgm);
  }

  // Runs *once* before the tests to build the dgm transmission graph
  static void SetUpTestSuite() {
    network = io::load_topology("../../tests/test_data/Sequential/topology.json");
    streams = io::load_time_triggered_traffic("../../tests/test_data/Sequential/streams.json");
    scheduled_streams = io::load_schedule("../../tests/test_data/Sequential/transmission_output.json");
    emergency_streams = io::load_emergency_traffic("../../tests/test_data/Sequential/emergency_streams.json");

    network.update_all_edges_aggregated_et_usage(emergency_streams);
  }

public:
  tsndgm::DisjunctiveGraphModel dgm;
};

// Define static members
tsndgm::NetworkTopology CalculationOutputTestSequential::network;
std::unordered_map<tsndgm::StreamID, tsndgm::MessageStream> CalculationOutputTestSequential::streams;
std::vector<tsndgm::EmergencyStream> CalculationOutputTestSequential::emergency_streams;
std::vector<tsndgm::StreamSchedule> CalculationOutputTestSequential::scheduled_streams;

// According to https://github.com/google/googletest/blob/main/docs/advanced.md this should make dgm available to the
// tests. But it does not seem to work, maybe because it is not initialized above?
// tsndgm::DisjunctiveGraphModel dgmTest::dgm;

TEST_F(CalculationOutputTestSequential, openCloseTimes) {
  const auto prop = dgm.transmission_graph[boost::graph_bundle];

  auto e = tsndgm::Edge(1, 0);
  auto v_opt = dgm.get_operation_on_edge(e, 0, 0);
  ASSERT_TRUE(v_opt.has_value());
  auto openings = prop.gate_openings[v_opt.value()];
  EXPECT_EQ(openings.first, 200);
  EXPECT_EQ(openings.second, 12956);


  e = tsndgm::Edge(0, 3);
  v_opt = dgm.get_operation_on_edge(e, 0, 0);
  ASSERT_TRUE(v_opt.has_value());
  openings = prop.gate_openings[v_opt.value()];
  EXPECT_EQ(openings.first, 24403);
  EXPECT_EQ(openings.second, 33356);
}

TEST_F(CalculationOutputTestSequential, weightsConjunctive) {
  const auto prop = dgm.transmission_graph[boost::graph_bundle];

  // Transmission Edge 1-0
  auto e = tsndgm::Edge(1, 0);
  auto v_opt = dgm.get_operation_on_edge(e, 0, 0);
  ASSERT_TRUE(v_opt.has_value());
  E transmission_e = dgm.getOutgoingConjunctiveEdge(v_opt.value()).value();
  EXPECT_EQ(dgm.transmission_graph[transmission_e].weight, 24203);

  // Transmission Edge 0-3
  e = tsndgm::Edge(0, 3);
  v_opt = dgm.get_operation_on_edge(e, 0, 0);
  ASSERT_TRUE(v_opt.has_value());
  transmission_e = dgm.getOutgoingConjunctiveEdge(v_opt.value()).value();
  EXPECT_EQ(dgm.transmission_graph[transmission_e].weight, 8200);
}