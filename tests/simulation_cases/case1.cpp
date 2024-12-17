#include "dgm/critical_path.h"
#include <IO/inputLoader.h>
#include <dgm/dgm.h>
#include <dgm/graph_visitors.h>
#include <gtest/gtest.h>

/**
 * Test case based on the omnet simulation.
 */

auto check_for_consecutive_edges(const tsndgm::DisjunctiveGraphModel &dgm) -> void {
  // ensure consecutive vertices have increasing critical cost and gate openings
  const auto &transmission_graph = dgm.transmission_graph;
  const auto &prop = transmission_graph[boost::graph_bundle];
  for (const auto edge : transmission_graph.m_edges | std::views::filter([&](const auto &e) {
                           return e.get_property().edge_type != tsndgm::fifo;
                         }) | std::views::filter([&](const auto &e) { return e.m_target != prop.sink; })) {
    const auto source_v = source(edge, transmission_graph);
    const auto destination_v = target(edge, transmission_graph);

    const auto source_crit_cost = prop.crit_cost[source_v];
    const auto destination_crit_cost = prop.crit_cost[destination_v];
    EXPECT_LT(source_crit_cost, destination_crit_cost);

    const auto source_gate_opening = prop.gate_openings[source_v].first;
    const auto destination_gate_opening = prop.gate_openings[destination_v].first;
    EXPECT_LT(source_gate_opening, destination_gate_opening);

    const auto source_gate_closing = prop.gate_openings[source_v].second;
    const auto destination_gate_closing = prop.gate_openings[destination_v].second;
    EXPECT_LT(source_gate_closing, destination_gate_closing);
  }
}

TEST(Case1, simulation_case_1) {
  auto network = io::load_topology("../../tests/test_data/simulation_case_1/topology.json");
  auto tt_streams = io::load_time_triggered_traffic("../../tests/test_data/simulation_case_1/streams.json");
  const auto scheduled_streams = io::load_schedule("../../tests/test_data/simulation_case_1/cp_out.json");
  const auto et_streams = io::load_emergency_traffic("../../tests/test_data/simulation_case_1/streams_et_1.5.json");

  io::set_routes(scheduled_streams, tt_streams);
  network.update_all_edges_aggregated_et_usage(et_streams);

  auto dgm = tsndgm::DisjunctiveGraphModel(network, tt_streams, scheduled_streams);
  tsndgm::critical_path::compute_longest_paths(dgm);

  //dgm.print();
  check_for_consecutive_edges(dgm);

  /*
   * check specific path that was buggy in the simulation:
   *  For stream 3:
   *  n9 -> n3 -> n0 (-> n1)
   *  For streams 5
   *  n9 -> n3 -> n4
   */
  // transmission from stream 3, but network link was also used by stream 5
  // Stream 3: frame 0: 53 & 54, frame 1: 57 & 58
  // Stream 5: frame 0: 74, frame 1: 77
  auto n9_n3_index = 53;
  auto n3_n0_index = 54;
  auto n3_n4_index = 74;
  const auto n9_n3 = dgm.transmission_graph.m_vertices[n9_n3_index];
  // stream 3 branches
  const auto n3_n0 = dgm.transmission_graph.m_vertices[n3_n0_index];
  // const auto n0_n1 = dgm.transmission_graph.m_vertices[55];
  // stream 5 branches
  const auto n3_n4 = dgm.transmission_graph.m_vertices[n3_n4_index];

  const auto &first_link = network.get_data_link_property(n9_n3.m_property.edge);
  const auto &s3_second_link = network.get_data_link_property(n3_n0.m_property.edge);
  const auto &s5_second_link = network.get_data_link_property(n3_n4.m_property.edge);

  auto [s3_bucket_size, s3_rate] = tsndgm::longest_path_visitor::getBranchingBurst(first_link, s3_second_link);
  // branching et streams for S3: 2 6 8
  const auto &et_2 = et_streams.at(2);
  const auto &et_6 = et_streams.at(6);
  const auto &et_8 = et_streams.at(8);
  EXPECT_DOUBLE_EQ(s3_bucket_size, et_2.bucket_size_byte + et_6.bucket_size_byte + et_8.bucket_size_byte);
  EXPECT_DOUBLE_EQ(s3_rate, et_2.refill_rate + et_6.refill_rate + et_8.refill_rate);

  auto [s5_bucket_size, s5_rate] = tsndgm::longest_path_visitor::getBranchingBurst(first_link, s5_second_link);
  // branching et streams for S5: 0
  const auto &et_0 = et_streams.at(0);
  EXPECT_DOUBLE_EQ(s5_bucket_size, et_0.bucket_size_byte);
  EXPECT_DOUBLE_EQ(s5_rate, et_0.refill_rate);

  const auto &gate_openings = dgm.transmission_graph[boost::graph_bundle].gate_openings;
  const auto &s3_gate_openings_link_1 = gate_openings[n9_n3_index];
  const auto &s3_gate_openings_link_2 = gate_openings[n3_n0_index];
  // 36881 is the gate closing reported in the simulation.
  EXPECT_LT(s3_gate_openings_link_1.first, s3_gate_openings_link_2.first);
  EXPECT_LT(s3_gate_openings_link_1.second, s3_gate_openings_link_2.second);
}
