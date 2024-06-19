#include <IO/inputLoader.h>
#include <gtest/gtest.h>
#include <network/topology.h>

auto assert_ids_match(const std::vector<std::reference_wrapper<const tsndgm::EmergencyStream>> &lhs,
                      const std::vector<tsndgm::StreamID> &rhs) {
  ASSERT_EQ(lhs.size(), rhs.size());
  for (size_t i = 0; i < lhs.size(); i++) {
    ASSERT_EQ(lhs.at(i).get().id, rhs.at(i));
  }
}

TEST(TopologyTest, aggregatetEmergencySpecifications) {
  const std::filesystem::path file_path_network = "../../tests/test_data/simple_topology.json";
  const std::filesystem::path file_path_emergency = "../../tests/test_data/emergency_streams.json";

  auto et_streams = io::load_emergency_traffic(file_path_emergency);
  auto topology = io::load_topology(file_path_network);

  topology.update_all_edges_aggregated_et_usage(et_streams);

  // Check edges
  const auto &edge_1 = topology.get_data_link_property({3, 1});
  ASSERT_EQ(edge_1.aggregated_emergency_refill_rate, 300000);
  ASSERT_EQ(edge_1.aggregated_emergency_burst_size, 17000);
  std::vector<tsndgm::StreamID> stream_ids_e31 = {0, 1, 2};
  assert_ids_match(edge_1.emergency_streams, stream_ids_e31);

  const auto &edge_2 = topology.get_data_link_property({1, 0});
  ASSERT_EQ(edge_2.aggregated_emergency_refill_rate, 300000);
  ASSERT_EQ(edge_2.aggregated_emergency_burst_size, 17000);
  std::vector<tsndgm::StreamID> stream_ids_e10 = {0, 1, 2};
  assert_ids_match(edge_2.emergency_streams, stream_ids_e10);

  const auto &edge_3 = topology.get_data_link_property({0, 2});
  ASSERT_EQ(edge_3.aggregated_emergency_refill_rate, 300000);
  ASSERT_EQ(edge_3.aggregated_emergency_burst_size, 17000);
  std::vector<tsndgm::StreamID> stream_ids_e02 = {0, 1, 2};
  assert_ids_match(edge_3.emergency_streams, stream_ids_e02);

  const auto &edge_4 = topology.get_data_link_property({2, 0});
  ASSERT_EQ(edge_4.aggregated_emergency_refill_rate, 175000);
  ASSERT_EQ(edge_4.aggregated_emergency_burst_size, 5000);
  std::vector<tsndgm::StreamID> stream_ids_e20 = {3};
  assert_ids_match(edge_4.emergency_streams, stream_ids_e20);

  const auto &edge_5 = topology.get_data_link_property({0, 1});
  ASSERT_EQ(edge_5.aggregated_emergency_refill_rate, 175000);
  ASSERT_EQ(edge_5.aggregated_emergency_burst_size, 5000);
  std::vector<tsndgm::StreamID> stream_ids_e01 = {3};
  assert_ids_match(edge_5.emergency_streams, stream_ids_e01);

  const auto &edge_6 = topology.get_data_link_property({1, 3});
  ASSERT_EQ(edge_6.aggregated_emergency_refill_rate, 175000);
  ASSERT_EQ(edge_6.aggregated_emergency_burst_size, 5000);
  std::vector<tsndgm::StreamID> stream_ids_e13 = {3};
  assert_ids_match(edge_6.emergency_streams, stream_ids_e13);
}
