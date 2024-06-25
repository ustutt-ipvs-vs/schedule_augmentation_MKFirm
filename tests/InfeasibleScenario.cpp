#include <IO/inputLoader.h>
#include <dgm/critical_path.h>
#include <dgm/dgm.h>
#include <gtest/gtest.h>

TEST(InfeasibleScenario, test1) {
  testing::internal::CaptureStdout();

  auto network = io::load_topology("../../tests/test_data/infeasible/topology.json");
  auto streams = io::load_time_triggered_traffic("../../tests/test_data/infeasible/streams.json");
  const auto scheduled_streams = io::load_schedule("../../tests/test_data/infeasible/cp_schedule.json");
  const auto emergency_streams = io::load_emergency_traffic("../../tests/test_data/infeasible/emergency_streams.json");

  io::set_routes(scheduled_streams, streams);
  network.update_all_edges_aggregated_et_usage(emergency_streams);

  tsndgm::DisjunctiveGraphModel dgm(network, streams, scheduled_streams);
  ASSERT_EQ(boost::num_vertices(dgm.transmission_graph), 4);
  ASSERT_EQ(boost::num_edges(dgm.transmission_graph), 5);

  tsndgm::critical_path::compute_longest_paths(dgm);
  ASSERT_TRUE(dgm.checkDeadlineViolations());

  std::string output = testing::internal::GetCapturedStdout();
}