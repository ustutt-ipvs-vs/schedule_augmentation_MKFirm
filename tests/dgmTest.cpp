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

  typedef boost::graph_traits<tsndgm::transmission_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<tsndgm::transmission_graph_t>::edge_descriptor E;
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

static unsigned int numberEdges(tsndgm::transmission_graph_t transmission_graph, tsndgm::TransmissionGraphEdgeType type) {
  unsigned int count = 0;
  for (auto e : boost::make_iterator_range(boost::edges(transmission_graph))) {
    if (transmission_graph[e].edge_type == type) {
      count++;
    }
  }
  return count;
}

TEST_F(dgmTest, numberVertices) {
  ASSERT_EQ(boost::num_vertices(dgm.transmission_graph), 17);
}

TEST_F(dgmTest, numberTotalEdges) {
  ASSERT_EQ(boost::num_edges(dgm.transmission_graph), 36);
}

TEST_F(dgmTest, numberConjunctiveEdges) {
  ASSERT_EQ(numberEdges(dgm.transmission_graph, tsndgm::conjunctive), 20);
}

TEST_F(dgmTest, numberDisjunctiveEdges) {
  ASSERT_EQ(numberEdges(dgm.transmission_graph, tsndgm::disjunctive), 12);
}

TEST_F(dgmTest, numberFifoEdges) {
  ASSERT_EQ(numberEdges(dgm.transmission_graph, tsndgm::fifo), 4);
}

TEST_F(dgmTest, conjuctiveEdges) {
  const auto e = tsndgm::Edge(0,2);
  const auto vertices = dgm.transmission_graph[boost::graph_bundle].topology_edge_to_dgm_vertices[e];
  V current_v;
  E current_e;

  for(const auto v : vertices){
    if(dgm.transmission_graph[v].stream_id == 2 && dgm.transmission_graph[v].frame_number == 0) {
      current_v = v;
    }
  }

  // Edge 1
  current_e = dgm.getOutgoingDisjunctiveEdge(current_v);
  ASSERT_EQ(dgm.transmission_graph[current_e].weight, 8504);

  current_v = target((current_e), dgm.transmission_graph);
  ASSERT_EQ(dgm.transmission_graph[current_v].stream_id, 0);
  ASSERT_EQ(dgm.transmission_graph[current_v].frame_number, 0);

  // Edge 2
  current_e = dgm.getOutgoingDisjunctiveEdge(current_v);
  ASSERT_EQ(dgm.transmission_graph[current_e].weight, 10624);

  current_v = target((current_e), dgm.transmission_graph);
  ASSERT_EQ(dgm.transmission_graph[current_v].stream_id, 1);
  ASSERT_EQ(dgm.transmission_graph[current_v].frame_number, 0);
}

TEST_F(dgmTest, fifoEdges) {
  const auto e_source = tsndgm::Edge(0,2);
  const auto vertices_source = dgm.transmission_graph[boost::graph_bundle].topology_edge_to_dgm_vertices[e_source];
  V source_1;
  V source_2;

  for(const auto v : vertices_source){
    if(dgm.transmission_graph[v].stream_id == 1){
      source_1 = v;
    }
    if(dgm.transmission_graph[v].stream_id == 2 && dgm.transmission_graph[v].frame_number == 0) {
      source_2 = v;
    }
  }

  // Edge 1
  const auto e_1 = boost::edge(source_1, target(dgm.getOutgoingFifoEdge(source_1), dgm.transmission_graph), dgm.transmission_graph);
  ASSERT_EQ(e_1.second, true);  
  ASSERT_EQ(dgm.transmission_graph[e_1.first].edge_type, tsndgm::fifo);
  ASSERT_EQ(dgm.transmission_graph[e_1.first].weight, 1096);

  // Edge 2
  const auto e_2 = boost::edge(source_2, target(dgm.getOutgoingFifoEdge(source_2), dgm.transmission_graph), dgm.transmission_graph);
  ASSERT_EQ(e_2.second, true);  
  ASSERT_EQ(dgm.transmission_graph[e_2.first].edge_type, tsndgm::fifo);
  ASSERT_EQ(dgm.transmission_graph[e_2.first].weight, -3496);
}

TEST_F(dgmTest, weightEdgesFromSrc) {
    V src = dgm.transmission_graph[boost::graph_bundle].src;
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

TEST_F(dgmTest, weightEdgesToSink) {
    V sink = dgm.transmission_graph[boost::graph_bundle].sink;
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
