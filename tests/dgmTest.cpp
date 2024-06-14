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

TEST_F(dgmTest, weightEdgesFromSrcTest) {
    auto src = dgm.transmission_graph[boost::graph_bundle].src;
    using boost::make_iterator_range;

    for (auto current_edge : make_iterator_range(out_edges(src, dgm.transmission_graph))){
      auto targetV = dgm.transmission_graph[boost::target (current_edge, dgm.transmission_graph)];

      if(targetV.stream_id == 0 && targetV.frame_number == 0){
        ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 17400);
      }
      else if(targetV.stream_id == 0 && targetV.frame_number == 1){
        ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 117400);
      }
      else if(targetV.stream_id == 1 && targetV.frame_number == 0){
        ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 38840);
      }
      else if(targetV.stream_id == 2 && targetV.frame_number == 0){
        ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 200);
      }
      else if(targetV.stream_id == 2 && targetV.frame_number == 1){
        ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 100200);
      }
      else{
        FAIL() << "Found edge from src that does not match any stream and frame number! Stream_id: " << targetV.stream_id << ", Frame_number: " << targetV.frame_number;
      }
    }
}

TEST_F(dgmTest, weightEdgesToSinkTest) {
    auto sink = dgm.transmission_graph[boost::graph_bundle].sink;
    using boost::make_iterator_range;

    for (auto current_edge : make_iterator_range(in_edges(sink, dgm.transmission_graph))){
      auto sourceV = dgm.transmission_graph[boost::source (current_edge, dgm.transmission_graph)];

      if(sourceV.stream_id == 0 && sourceV.frame_number == 0){
        ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 10724);
      }
      else if(sourceV.stream_id == 0 && sourceV.frame_number == 1){
        ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 10724);
      }
      else if(sourceV.stream_id == 1 && sourceV.frame_number == 0){
        ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 10900);
      }
      else if(sourceV.stream_id == 2 && sourceV.frame_number == 0){
        ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 8604);
      }
      else if(sourceV.stream_id == 2 && sourceV.frame_number == 1){
        ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 8604);
      }
      else{
        FAIL() << "Found edge to sink that does not match any stream and frame number! Stream_id: " << sourceV.stream_id << ", Frame_number: " << sourceV.frame_number;
      }
    }

}
