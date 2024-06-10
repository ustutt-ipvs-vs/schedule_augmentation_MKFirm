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

    // TODO check if edges exist
}
