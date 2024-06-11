#include <gtest/gtest.h>
#include <network/topology.h>

TEST(TopologyTest, loadSimpleTopologyTest)
{
    const std::filesystem::path file_path = "../../tests/test_data/simple_topology.json";

    const auto topology = tsndgm::NetworkTopology(file_path);

    bool failure = false;
    for (tsndgm::DeviceId i = 0; i < 4; i++)
    {
        const auto result = topology.exists(i);
        EXPECT_TRUE(result) << "Device " << i << " does not exist";

        failure = std::max(failure, not result);
    }
    ASSERT_FALSE(failure);
    EXPECT_FALSE(topology.exists(4)) << "Device " << 4 << " should not exist";

    // Check devices
    const auto &device_0 = topology[0];
    ASSERT_EQ(device_0.id, 0);
    ASSERT_EQ(device_0.name, "n0");
    ASSERT_EQ(device_0.processing_delay, 2000);
    ASSERT_EQ(device_0.queues_per_port, 8);
    // TODO add a check for is_switch as soon as it is implemented


    const std::vector<tsndgm::Edge> edges = {{0, 1}, {1, 0}, {0, 2}, {2, 0}, {1, 3}, {3, 1}};
    for (const auto &edge : edges)
    {
        EXPECT_TRUE(topology.exists(edge)) << "Edge " << edge.first << " -> " << edge.second << " does not exist";
    }
    EXPECT_FALSE(topology.exists({0, 3})) << "Edge " << 0 << " -> " << 3 << " should not exist";
}
