#include <IO/inputLoader.h>
#include <gtest/gtest.h>
#include <util/constants.h>

TEST(InputLoaderTest, loadEmergencyStreamTest)
{
    const std::filesystem::path file_path = "../../tests/test_data/emergency_streams.json";

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

TEST(InputLoaderTest, loadTimeTriggeredTraffic)
{
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

TEST(InputLoaderTest, filesDoNotExist)
{
    // emergency streams
    const std::filesystem::path file_path = "../../tests/test_data/does_not_exist.json";

    EXPECT_EXIT(io::load_emergency_traffic(file_path), testing::ExitedWithCode(error_codes::FILE_NOT_FOUND), ".*");

    // TODO implement the others
}
