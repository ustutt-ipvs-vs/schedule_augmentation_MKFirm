#include <IO/inputLoader.h>
#include <dgm/dgm.h>
#include <gtest/gtest.h>
#include <util/constants.h>

// fixture class
class dgmTest : public testing::Test {
public:
  static tsndgm::NetworkTopology network;
  static std::unordered_map<tsndgm::StreamID, tsndgm::MessageStream> streams;
  static std::vector<tsndgm::StreamSchedule> scheduled_streams;

protected:
  dgmTest() : dgm(network, streams, scheduled_streams) {
    // Code in this constructor will be executed before *each* test

    // For debugging, this should print the dgm before each test, if it is initialized correctly
    dgm.print();
  }

  // Runs *once* before the tests to build the dgm transmission graph
  static void SetUpTestSuite() {
    network = io::load_topology("../../tests/test_data/small_topology.json");
    streams = io::load_time_triggered_traffic("../../tests/test_data/small_streams.json");
    scheduled_streams = io::load_schedule("../../tests/test_data/small_transmission_output.json");
  }

public:
  tsndgm::DisjunctiveGraphModel dgm;
};

// Define static members
tsndgm::NetworkTopology dgmTest::network;
std::unordered_map<tsndgm::StreamID, tsndgm::MessageStream> dgmTest::streams;
std::vector<tsndgm::StreamSchedule> dgmTest::scheduled_streams;

// According to https://github.com/google/googletest/blob/main/docs/advanced.md this should make dgm available to the
// tests. But it does not seem to work, maybe because it is not initialized above?
// tsndgm::DisjunctiveGraphModel dgmTest::dgm;

TEST_F(dgmTest, operationVerticesTest) {}

TEST_F(dgmTest, conjunctiveEdgesTest) {}
