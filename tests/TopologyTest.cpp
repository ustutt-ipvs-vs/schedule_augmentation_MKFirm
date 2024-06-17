#include <IO/inputLoader.h>
#include <gtest/gtest.h>
#include <network/topology.h>

TEST(TopologyTest, aggregatetEmergencySpezifikations) {
  const std::filesystem::path file_path_network =
      "../../tests/test_data/simple_topology.json";
  const std::filesystem::path file_path_emergency =
      "../../tests/test_data/emergency_streams.json";

  auto et_streams = io::load_emergency_traffic(file_path_emergency);
  auto topology = io::load_topology(file_path_network);

  topology.calculate_aggregated_emergency_usage(et_streams);

  // Check edges
  const auto &edge_1 = topology.get_data_link_property({3,1});
  ASSERT_EQ(edge_1.aggregated_emergency_refill_rate, 300000);
  ASSERT_EQ(edge_1.aggregated_emergency_burst_size, 17000);

  const auto &edge_2 = topology.get_data_link_property({1,0});
  ASSERT_EQ(edge_2.aggregated_emergency_refill_rate, 300000);
  ASSERT_EQ(edge_2.aggregated_emergency_burst_size, 17000);

  const auto &edge_3 = topology.get_data_link_property({0,2});
  ASSERT_EQ(edge_3.aggregated_emergency_refill_rate, 300000);
  ASSERT_EQ(edge_3.aggregated_emergency_burst_size, 17000);

  const auto &edge_4 = topology.get_data_link_property({1,3});
  ASSERT_EQ(edge_4.aggregated_emergency_refill_rate, 175000);
  ASSERT_EQ(edge_4.aggregated_emergency_burst_size, 5000);

  const auto &edge_5 = topology.get_data_link_property({0,1});
  ASSERT_EQ(edge_5.aggregated_emergency_refill_rate, 175000);
  ASSERT_EQ(edge_5.aggregated_emergency_burst_size, 5000);

  const auto &edge_6 = topology.get_data_link_property({1,3});
  ASSERT_EQ(edge_6.aggregated_emergency_refill_rate, 175000);
  ASSERT_EQ(edge_6.aggregated_emergency_burst_size, 5000);
}
