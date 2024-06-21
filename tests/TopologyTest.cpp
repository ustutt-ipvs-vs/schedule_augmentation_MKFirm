#include <IO/inputLoader.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <network/topology.h>

auto assert_ids_match(const std::vector<std::reference_wrapper<const tsndgm::EmergencyStream>> &lhs,
                      const std::vector<tsndgm::StreamID> &rhs) {
  ASSERT_EQ(lhs.size(), rhs.size());
  for (const auto &[lhs_elem, rhs_elem] : std::views::zip(lhs, rhs)) {
    EXPECT_EQ(lhs_elem.get().id, rhs_elem);
  }
}

template <typename T>
auto IsInRange(T value, int precision) {
  return testing::AllOf(testing::Ge((value - precision)), testing::Le((value + precision)));
}



TEST(TopologyTest, aggregatetEmergencySpecifications) {
  const std::filesystem::path file_path_network = "../../tests/test_data/simple_topology.json";
  const std::filesystem::path file_path_emergency = "../../tests/test_data/emergency_streams.json";

  auto et_streams = io::load_emergency_traffic(file_path_emergency);
  auto topology = io::load_topology(file_path_network);


  topology.update_all_edges_aggregated_et_usage(et_streams);

  // Check edges
  const auto &edge_1 = topology.get_data_link_property({3, 1});

  ASSERT_THAT(edge_1.aggregated_emergency_refill_rate, IsInRange(300000 * tsndgm::INTER_FRAME_GAP_INCREASE_FACTOR, 2));
  ASSERT_THAT(edge_1.aggregated_emergency_burst_size, IsInRange(17000 * tsndgm::INTER_FRAME_GAP_INCREASE_FACTOR, 2));
  std::vector<tsndgm::StreamID> stream_ids_e31 = {0, 1, 2};
  assert_ids_match(edge_1.emergency_streams, stream_ids_e31);

  const auto &edge_2 = topology.get_data_link_property({1, 0});
  ASSERT_THAT(edge_2.aggregated_emergency_refill_rate, IsInRange(300000 * tsndgm::INTER_FRAME_GAP_INCREASE_FACTOR, 2));
  ASSERT_THAT(edge_2.aggregated_emergency_burst_size, IsInRange(17000 * tsndgm::INTER_FRAME_GAP_INCREASE_FACTOR, 2));
  std::vector<tsndgm::StreamID> stream_ids_e10 = {0, 1, 2};
  assert_ids_match(edge_2.emergency_streams, stream_ids_e10);

  const auto &edge_3 = topology.get_data_link_property({0, 2});
  ASSERT_THAT(edge_3.aggregated_emergency_refill_rate, IsInRange(300000 * tsndgm::INTER_FRAME_GAP_INCREASE_FACTOR, 2));
  ASSERT_THAT(edge_3.aggregated_emergency_burst_size, IsInRange(17000 * tsndgm::INTER_FRAME_GAP_INCREASE_FACTOR, 2));
  std::vector<tsndgm::StreamID> stream_ids_e02 = {0, 1, 2};
  assert_ids_match(edge_3.emergency_streams, stream_ids_e02);

  const auto &edge_4 = topology.get_data_link_property({2, 0});
  ASSERT_THAT(edge_4.aggregated_emergency_refill_rate, IsInRange(175000 * tsndgm::INTER_FRAME_GAP_INCREASE_FACTOR, 2));
  ASSERT_THAT(edge_4.aggregated_emergency_burst_size, IsInRange(5000 * tsndgm::INTER_FRAME_GAP_INCREASE_FACTOR, 2));
  std::vector<tsndgm::StreamID> stream_ids_e20 = {3};
  assert_ids_match(edge_4.emergency_streams, stream_ids_e20);

  const auto &edge_5 = topology.get_data_link_property({0, 1});
  ASSERT_THAT(edge_5.aggregated_emergency_refill_rate, IsInRange(175000 * tsndgm::INTER_FRAME_GAP_INCREASE_FACTOR, 2));
  ASSERT_THAT(edge_5.aggregated_emergency_burst_size, IsInRange(5000 * tsndgm::INTER_FRAME_GAP_INCREASE_FACTOR, 2));
  std::vector<tsndgm::StreamID> stream_ids_e01 = {3};
  assert_ids_match(edge_5.emergency_streams, stream_ids_e01);

  const auto &edge_6 = topology.get_data_link_property({1, 3});
  ASSERT_THAT(edge_6.aggregated_emergency_refill_rate, IsInRange(175000 * tsndgm::INTER_FRAME_GAP_INCREASE_FACTOR, 2));
  ASSERT_THAT(edge_6.aggregated_emergency_burst_size, IsInRange(5000 * tsndgm::INTER_FRAME_GAP_INCREASE_FACTOR, 2));
  std::vector<tsndgm::StreamID> stream_ids_e13 = {3};
  assert_ids_match(edge_6.emergency_streams, stream_ids_e13);
}
