#include <IO/inputLoader.h>
#include <gtest/gtest.h>
#include <util/constants.h>

TEST(InputLoaderTest, loadEmergencyStreamTest) {
  const std::filesystem::path file_path =
      "../../tests/test_data/emergency_streams.json";

  const auto et_streams = io::load_emergency_traffic(file_path);

  ASSERT_EQ(et_streams.size(), 4);

  // stream 0
  const tsndgm::EmergencyStream &stream_0 = et_streams.at(0);
  EXPECT_EQ(stream_0.id, 0);
  EXPECT_EQ(stream_0.name, "emergency_stream_0");
  EXPECT_EQ(stream_0.source, 3);
  EXPECT_EQ(stream_0.destination, 2);
  EXPECT_EQ(stream_0.bucket_size_byte, 9000);
  EXPECT_EQ(stream_0.refill_rate, tsndgm::mbps_to_DataRate(0.1));

  // stream 3
  const tsndgm::EmergencyStream &stream_3 = et_streams.at(3);
  EXPECT_EQ(stream_3.id, 3);
  EXPECT_EQ(stream_3.name, "emergency_stream_3");
  EXPECT_EQ(stream_3.source, 2);
  EXPECT_EQ(stream_3.destination, 3);
  EXPECT_EQ(stream_3.bucket_size_byte, 5000);
  EXPECT_EQ(stream_3.refill_rate, tsndgm::mbps_to_DataRate(1.4));
}

TEST(InputLoaderTest, loadTimeTriggeredTraffic) {
  const std::filesystem::path file_path = "../../tests/test_data/streams.json";

  const auto tt_streams = io::load_time_triggered_traffic(file_path);

  ASSERT_EQ(tt_streams.size(), 4);

  // stream 0
  const tsndgm::MessageStream &stream_0 = tt_streams.at(0);
  EXPECT_EQ(stream_0.id, 0);
  EXPECT_EQ(stream_0.name, "stream_0");
  EXPECT_EQ(stream_0.period, 100000);
  EXPECT_EQ(stream_0.frame_size, 847);
  EXPECT_EQ(stream_0.deadline, 82250);
  EXPECT_EQ(stream_0.jitter, 0);
  EXPECT_EQ(stream_0.phase, 0);
  EXPECT_EQ(stream_0.route.source, 3);
  EXPECT_EQ(stream_0.route.destination, 2);

  // stream 3
  const tsndgm::MessageStream &stream_3 = tt_streams.at(3);
  EXPECT_EQ(stream_3.id, 3);
  EXPECT_EQ(stream_3.name, "stream_3");
  EXPECT_EQ(stream_3.period, 100000);
  EXPECT_EQ(stream_3.frame_size, 1063);
  EXPECT_EQ(stream_3.deadline, 67500);
  EXPECT_EQ(stream_3.jitter, 0);
  EXPECT_EQ(stream_3.phase, 0);
  EXPECT_EQ(stream_3.route.source, 3);
  EXPECT_EQ(stream_3.route.destination, 2);
}

TEST(InputLoaderTest, loadSimpleTopologyTest) {
  const std::filesystem::path file_path =
      "../../tests/test_data/simple_topology.json";

  const auto topology = io::load_topology(file_path);

  bool failure = false;
  for (tsndgm::DeviceId i = 0; i < 4; i++) {
    const auto result = topology.exists(i);
    EXPECT_TRUE(result) << "Device " << i << " does not exist";

    failure = std::max(failure, not result);
  }
  ASSERT_FALSE(failure);
  EXPECT_FALSE(topology.exists(4)) << "Device " << 4 << " should not exist";

  // Check devices
  const auto &device_0 = topology.get_device_property(0);
  ASSERT_EQ(device_0.id, 0);
  ASSERT_EQ(device_0.name, "n0");
  ASSERT_EQ(device_0.processing_delay, 2000);
  ASSERT_EQ(device_0.queues_per_port, 8);
  // TODO add a check for is_switch as soon as it is implemented

  const std::vector<tsndgm::Edge> edges = {{0, 1}, {1, 0}, {0, 2},
                                           {2, 0}, {1, 3}, {3, 1}};
  for (const auto &edge : edges) {
    EXPECT_TRUE(topology.exists(edge))
        << "Edge " << edge.first << " -> " << edge.second << " does not exist";
  }
  EXPECT_FALSE(topology.exists({0, 3}))
      << "Edge " << 0 << " -> " << 3 << " should not exist";

  // Check edge
  const auto &edge = topology.get_data_link_property({0,1});
  ASSERT_EQ(edge.data_rate, tsndgm::mbps_to_DataRate(1000));
  ASSERT_EQ(edge.propagation_delay, 200);
}

TEST(InputLoaderTest, loadScheduleTest) {
  const std::filesystem::path file_path = "../../tests/test_data/schedule.json";

  const auto schedule = io::load_schedule(file_path);
  ASSERT_EQ(schedule.size(), 4);

  // stream 0
  const auto &stream_0 = schedule.at(0);
  EXPECT_EQ(stream_0.stream_id, 0);
  EXPECT_EQ(stream_0.pcp, 1);
  EXPECT_EQ(stream_0.frames.size(), 2);

  // stream 0 frame 0
  const auto &frame_0 = stream_0.frames.at(0);
  EXPECT_EQ(frame_0.frame_number, 0);
  EXPECT_EQ(frame_0.transmissions.size(), 3);

  // stream 0 frame 0 transmission 0
  const auto &transmission_0 = frame_0.transmissions.at(0);
  EXPECT_EQ(transmission_0.link_id, 5);
  EXPECT_EQ(transmission_0.link_name, "3-1");
  EXPECT_EQ(transmission_0.source, 3);
  EXPECT_EQ(transmission_0.target, 1);
  EXPECT_EQ(transmission_0.start, 200);
  EXPECT_EQ(transmission_0.end, 6976);
}

TEST(InputLoaderTest, filesDoNotExist) {
  testing::internal::CaptureStdout();
  const std::filesystem::path non_existing_file_path =
      "../../tests/test_data/does_not_exist.json";
  EXPECT_EXIT(io::load_emergency_traffic(non_existing_file_path),
              testing::ExitedWithCode(error_codes::FILE_NOT_FOUND), ".*");

  EXPECT_EXIT(io::load_time_triggered_traffic(non_existing_file_path),
              testing::ExitedWithCode(error_codes::FILE_NOT_FOUND), ".*");

  EXPECT_EXIT(io::load_schedule(non_existing_file_path),
              testing::ExitedWithCode(error_codes::FILE_NOT_FOUND), ".*");

  EXPECT_EXIT(io::load_topology(non_existing_file_path),
              testing::ExitedWithCode(error_codes::FILE_NOT_FOUND), ".*");
  std::string output = testing::internal::GetCapturedStdout();
}

TEST(InputLoaderTest, NotAValidJsonFile) {
  testing::internal::CaptureStdout();
  const std::filesystem::path invalid_json_file_path =
      "../../tests/test_data/invalid_json.json";
  EXPECT_EXIT(io::load_emergency_traffic(invalid_json_file_path),
              testing::ExitedWithCode(error_codes::JSON_PARSING_FAILED), ".*");

  EXPECT_EXIT(io::load_time_triggered_traffic(invalid_json_file_path),
              testing::ExitedWithCode(error_codes::JSON_PARSING_FAILED), ".*");

  EXPECT_EXIT(io::load_schedule(invalid_json_file_path),
              testing::ExitedWithCode(error_codes::JSON_PARSING_FAILED), ".*");

  EXPECT_EXIT(io::load_topology(invalid_json_file_path),
              testing::ExitedWithCode(error_codes::JSON_PARSING_FAILED), ".*");
  std::string output = testing::internal::GetCapturedStdout();
}
