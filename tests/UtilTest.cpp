#include <gtest/gtest.h>
#include <util/typedefs.h>

TEST(UtilTest, mbps_to_DataRate)
{

    EXPECT_EQ(tsndgm::mbps_to_DataRate(100), 12500000);
    EXPECT_EQ(tsndgm::mbps_to_DataRate(0), 0);
    EXPECT_EQ(tsndgm::mbps_to_DataRate(1), 125000);
    EXPECT_EQ(tsndgm::mbps_to_DataRate(0.5), 62500);
    EXPECT_EQ(tsndgm::mbps_to_DataRate(0.1), 12500);

    // negative values don't make sense, but the function should still work
    EXPECT_EQ(tsndgm::mbps_to_DataRate(-1), -125000);
}
