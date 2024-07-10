#include <IO/inputLoader.h>
#include <dgm/dgm.h>
#include <gtest/gtest.h>

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

static unsigned int numberEdges(tsndgm::transmission_graph_t transmission_graph,
                                const tsndgm::TransmissionGraphEdgeType type) {
  auto range = make_iterator_range(edges(transmission_graph));
  const auto count =
      std::ranges::count_if(range, [&](const auto e) { return transmission_graph[e].edge_type == type; });
  return count;
}

TEST_F(dgmTest, numberVertices) { ASSERT_EQ(boost::num_vertices(dgm.transmission_graph), 17); }

TEST_F(dgmTest, numberTotalEdges) { ASSERT_EQ(boost::num_edges(dgm.transmission_graph), 36); }

TEST_F(dgmTest, numberConjunctiveEdges) { ASSERT_EQ(numberEdges(dgm.transmission_graph, tsndgm::conjunctive), 20); }

TEST_F(dgmTest, numberDisjunctiveEdges) { ASSERT_EQ(numberEdges(dgm.transmission_graph, tsndgm::disjunctive), 12); }

TEST_F(dgmTest, numberFifoEdges) { ASSERT_EQ(numberEdges(dgm.transmission_graph, tsndgm::fifo), 4); }

TEST_F(dgmTest, numberOutgoingEdgesInnerVertices) {
  for (const auto v : boost::make_iterator_range(vertices(dgm.transmission_graph))) {
    if (v == dgm.transmission_graph[boost::graph_bundle].src || v == dgm.transmission_graph[boost::graph_bundle].sink) {
      break;
    }
    const auto count = make_iterator_range(out_edges(v, dgm.transmission_graph)).size();
    ASSERT_LE(count, 3);
  }
}

TEST_F(dgmTest, numberOutgoingEdgesSrc) {
  const auto count_e =
      make_iterator_range(out_edges(dgm.transmission_graph[boost::graph_bundle].src, dgm.transmission_graph)).size();
  unsigned long count_sol = 0;
  for (const auto &stream : dgm.scheduled_streams) {
    count_sol += stream.frames.size();
  }
  ASSERT_LE(count_e, count_sol);
}

TEST_F(dgmTest, conjunctiveEdges) {
  constexpr auto e = tsndgm::Edge(3, 1);
  const auto v_opt = dgm.get_operation_on_edge(e, 0, 0);
  ASSERT_TRUE(v_opt.has_value());
  auto current_v = v_opt.value();

  // Edge 1
  E current_e = dgm.getOutgoingConjunctiveEdge(current_v).value();
  ASSERT_EQ(dgm.transmission_graph[current_e].weight, 23448);

  current_v = target(current_e, dgm.transmission_graph);
  ASSERT_EQ(dgm.transmission_graph[current_v].stream_id, 0);
  ASSERT_EQ(dgm.transmission_graph[current_v].frame_number, 0);

  // Edge 2
  current_e = dgm.getOutgoingConjunctiveEdge(current_v).value();
  ASSERT_EQ(dgm.transmission_graph[current_e].weight, 11824);

  current_v = target(current_e, dgm.transmission_graph);
  ASSERT_EQ(dgm.transmission_graph[current_v].stream_id, 0);
  ASSERT_EQ(dgm.transmission_graph[current_v].frame_number, 0);
}

TEST_F(dgmTest, discjunctiveEdgesTopoEdge0_2) {
  constexpr auto e = tsndgm::Edge(0, 2);
  const auto v_opt = dgm.get_operation_on_edge(e, 2, 0);
  ASSERT_TRUE(v_opt.has_value());
  auto current_v = v_opt.value();

  // Edge 1
  E current_e = dgm.getOutgoingDisjunctiveEdge(current_v).value();
  ASSERT_EQ(dgm.transmission_graph[current_e].weight, 8600);

  current_v = target(current_e, dgm.transmission_graph);
  ASSERT_EQ(dgm.transmission_graph[current_v].stream_id, 0);
  ASSERT_EQ(dgm.transmission_graph[current_v].frame_number, 0);

  // Edge 2
  current_e = dgm.getOutgoingDisjunctiveEdge(current_v).value();
  ASSERT_EQ(dgm.transmission_graph[current_e].weight, 10720);

  current_v = target(current_e, dgm.transmission_graph);
  ASSERT_EQ(dgm.transmission_graph[current_v].stream_id, 1);
  ASSERT_EQ(dgm.transmission_graph[current_v].frame_number, 0);

  // Edge 3
  current_e = dgm.getOutgoingDisjunctiveEdge(current_v).value();
  ASSERT_EQ(dgm.transmission_graph[current_e].weight, 10896);

  current_v = target(current_e, dgm.transmission_graph);
  ASSERT_EQ(dgm.transmission_graph[current_v].stream_id, 2);
  ASSERT_EQ(dgm.transmission_graph[current_v].frame_number, 1);
}

TEST_F(dgmTest, discjunctiveEdgesTopoEdge3_1) {
  constexpr auto e = tsndgm::Edge(3, 1);
  const auto v_opt = dgm.get_operation_on_edge(e, 2, 0);
  ASSERT_TRUE(v_opt.has_value());
  auto current_v = v_opt.value();

  // Edge 1
  E current_e = dgm.getOutgoingDisjunctiveEdge(current_v).value();
  ASSERT_EQ(dgm.transmission_graph[current_e].weight, 17200);

  current_v = target(current_e, dgm.transmission_graph);
  ASSERT_EQ(dgm.transmission_graph[current_v].stream_id, 0);
  ASSERT_EQ(dgm.transmission_graph[current_v].frame_number, 0);

  // Edge 2
  current_e = dgm.getOutgoingDisjunctiveEdge(current_v).value();
  ASSERT_EQ(dgm.transmission_graph[current_e].weight, 21440);

  current_v = target(current_e, dgm.transmission_graph);
  ASSERT_EQ(dgm.transmission_graph[current_v].stream_id, 1);
  ASSERT_EQ(dgm.transmission_graph[current_v].frame_number, 0);

  // Edge 3
  current_e = dgm.getOutgoingDisjunctiveEdge(current_v).value();
  ASSERT_EQ(dgm.transmission_graph[current_e].weight, 21792);

  current_v = target(current_e, dgm.transmission_graph);
  ASSERT_EQ(dgm.transmission_graph[current_v].stream_id, 2);
  ASSERT_EQ(dgm.transmission_graph[current_v].frame_number, 1);
}

TEST_F(dgmTest, fifoEdges) {
  constexpr auto e_source = tsndgm::Edge(0, 2);
  const auto v1_opt = dgm.get_operation_on_edge(e_source, 1, 0);
  ASSERT_TRUE(v1_opt.has_value());
  const auto v2_opt = dgm.get_operation_on_edge(e_source, 2, 0);
  ASSERT_TRUE(v2_opt.has_value());

  const V source_1 = v1_opt.value();
  const V source_2 = v2_opt.value();

  // Edge 1
  const auto e_1 =
      edge(source_1, target(dgm.getOutgoingFifoEdge(source_1).value(), dgm.transmission_graph), dgm.transmission_graph);
  ASSERT_EQ(e_1.second, true);
  ASSERT_EQ(dgm.transmission_graph[e_1.first].edge_type, tsndgm::fifo);
  ASSERT_EQ(dgm.transmission_graph[e_1.first].weight, 1096);

  // Edge 2
  const auto e_2 =
      edge(source_2, target(dgm.getOutgoingFifoEdge(source_2).value(), dgm.transmission_graph), dgm.transmission_graph);
  ASSERT_EQ(e_2.second, true);
  ASSERT_EQ(dgm.transmission_graph[e_2.first].edge_type, tsndgm::fifo);
  ASSERT_EQ(dgm.transmission_graph[e_2.first].weight, -3496);
}

TEST_F(dgmTest, weightEdgesFromSrc) {
  const V src = dgm.transmission_graph[boost::graph_bundle].src;
  using boost::make_iterator_range;

  for (auto current_edge : make_iterator_range(out_edges(src, dgm.transmission_graph))) {
    if (const auto targetV = dgm.transmission_graph[target(current_edge, dgm.transmission_graph)];
        targetV.stream_id == 0 && targetV.frame_number == 0) {
      ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 17400);
    } else if (targetV.stream_id == 0 && targetV.frame_number == 1) {
      ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 117400);
    } else if (targetV.stream_id == 1 && targetV.frame_number == 0) {
      ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 38840);
    } else if (targetV.stream_id == 2 && targetV.frame_number == 0) {
      ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 200);
    } else if (targetV.stream_id == 2 && targetV.frame_number == 1) {
      ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 100200);
    } else {
      FAIL() << "Found edge from src that does not match any stream and frame number! Stream_id: " << targetV.stream_id
             << ", Frame_number: " << targetV.frame_number;
    }
  }
}

TEST_F(dgmTest, weightEdgesToSink) {
  const V sink = dgm.transmission_graph[boost::graph_bundle].sink;
  using boost::make_iterator_range;

  for (auto current_edge : make_iterator_range(in_edges(sink, dgm.transmission_graph))) {
    if (const auto sourceV = dgm.transmission_graph[source(current_edge, dgm.transmission_graph)];
        sourceV.stream_id == 0 && sourceV.frame_number == 0) {
      ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 10724);
    } else if (sourceV.stream_id == 0 && sourceV.frame_number == 1) {
      ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 10724);
    } else if (sourceV.stream_id == 1 && sourceV.frame_number == 0) {
      ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 10900);
    } else if (sourceV.stream_id == 2 && sourceV.frame_number == 0) {
      ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 8604);
    } else if (sourceV.stream_id == 2 && sourceV.frame_number == 1) {
      ASSERT_EQ(dgm.transmission_graph[current_edge].weight, 8604);
    } else {
      FAIL() << "Found edge to sink that does not match any stream and frame number! Stream_id: " << sourceV.stream_id
             << ", Frame_number: " << sourceV.frame_number;
    }
  }
}

TEST_F(dgmTest, hyperCycle) { EXPECT_EQ(dgm.hyper_cycle, 200000); }
