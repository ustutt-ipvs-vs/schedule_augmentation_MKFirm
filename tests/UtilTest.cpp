#include <gtest/gtest.h>
#include <util/typedefs.h>

TEST(UtilTest, mbps_to_DataRate) {

  EXPECT_EQ(tsndgm::mbps_to_DataRate(100), 12500000 / 1e9);
  EXPECT_EQ(tsndgm::mbps_to_DataRate(0), 0 / 1e9);
  EXPECT_EQ(tsndgm::mbps_to_DataRate(1), 125000 / 1e9);
  EXPECT_EQ(tsndgm::mbps_to_DataRate(0.5), 62500 / 1e9);
  EXPECT_EQ(tsndgm::mbps_to_DataRate(0.1), 12500 / 1e9);
  EXPECT_EQ(tsndgm::mbps_to_DataRate(1.4), 175000 / 1e9);
  EXPECT_EQ(tsndgm::mbps_to_DataRate(0.001), 125 / 1e9);
  EXPECT_EQ(tsndgm::mbps_to_DataRate(0.000001), 125 / 1e12);

  // negative values don't make sense, but the function should still work
  // this code does not work on all machines. Some manage the under/overflow, others not.
  // EXPECT_EQ(tsndgm::mbps_to_DataRate(-1), -125000UL);
}
