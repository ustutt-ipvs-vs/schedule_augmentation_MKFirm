#include "dgm/critical_path.h"
#include <IO/inputLoader.h>
#include <dgm/dgm.h>
#include <gtest/gtest.h>

// fixture class
class CalculationOutputTestSimplified : public testing::Test {
public:
    static tsndgm::NetworkTopology network;
    static std::unordered_map<tsndgm::StreamID, tsndgm::MessageStream> streams;
    static std::vector<tsndgm::StreamSchedule> scheduled_streams;
    static std::vector<tsndgm::EmergencyStream> emergency_streams;

    typedef boost::graph_traits<tsndgm::transmission_graph_t>::vertex_descriptor V;
    typedef boost::graph_traits<tsndgm::transmission_graph_t>::edge_descriptor E;

protected:
    CalculationOutputTestSimplified() : dgm(network, streams, scheduled_streams) {
        // Code in this constructor will be executed before *each* test
        tsndgm::critical_path::compute_longest_paths(dgm);
    }

    // Runs *once* before the tests to build the dgm transmission graph
    static void SetUpTestSuite() {
        network = io::load_topology("../../tests/test_data/Simplified_Setting/topology.json");
        streams = io::load_time_triggered_traffic("../../tests/test_data/Simplified_Setting/streams.json");
        scheduled_streams = io::load_schedule("../../tests/test_data/Simplified_Setting/transmission_output.json");
        emergency_streams = io::load_emergency_traffic(
                "../../tests/test_data/Simplified_Setting/emergency_streams.json");

        network.update_all_edges_aggregated_et_usage(emergency_streams);
    }

public:
    tsndgm::DisjunctiveGraphModel dgm;
};

// Define static members
tsndgm::NetworkTopology CalculationOutputTestSimplified::network;
std::unordered_map<tsndgm::StreamID, tsndgm::MessageStream> CalculationOutputTestSimplified::streams;
std::vector<tsndgm::EmergencyStream> CalculationOutputTestSimplified::emergency_streams;
std::vector<tsndgm::StreamSchedule> CalculationOutputTestSimplified::scheduled_streams;

// According to https://github.com/google/googletest/blob/main/docs/advanced.md this should make dgm available to the
// tests. But it does not seem to work, maybe because it is not initialized above?
// tsndgm::DisjunctiveGraphModel dgmTest::dgm;

TEST_F(CalculationOutputTestSimplified, openCloseTimesSimplified) {
    constexpr auto e = tsndgm::Edge(0, 1);
    const auto prop = dgm.transmission_graph[boost::graph_bundle];

    // Stream 0
    auto v_opt = dgm.get_operation_on_edge(e, 0, 0);
    ASSERT_TRUE(v_opt.has_value());
    auto openings = prop.gate_openings[v_opt.value()];
    EXPECT_EQ(openings.first, 0);
    EXPECT_EQ(openings.second, 265372);

    // Stream 1
    v_opt = dgm.get_operation_on_edge(e, 1, 0);
    ASSERT_TRUE(v_opt.has_value());
    openings = prop.gate_openings[v_opt.value()];
    EXPECT_EQ(openings.first, 155613);
    EXPECT_EQ(openings.second, 422985);

    // Stream 2
    v_opt = dgm.get_operation_on_edge(e, 2, 0);
    ASSERT_TRUE(v_opt.has_value());
    openings = prop.gate_openings[v_opt.value()];
    EXPECT_EQ(openings.first, 311226);
    EXPECT_EQ(openings.second, 565958);
}

TEST_F(CalculationOutputTestSimplified, weightsDisjunctiveSimplified) {
    constexpr auto e = tsndgm::Edge(0, 1);
    const auto prop = dgm.transmission_graph[boost::graph_bundle];

    // Edge 0
    auto v_opt = dgm.get_operation_on_edge(e, 0, 0);
    ASSERT_TRUE(v_opt.has_value());
    E transmission_e = dgm.getOutgoingDisjunctiveEdge(v_opt.value()).value();
    EXPECT_EQ(dgm.transmission_graph[transmission_e].weight, 155613);

    // Edge 1
    v_opt = dgm.get_operation_on_edge(e, 1, 0);
    ASSERT_TRUE(v_opt.has_value());
    transmission_e = dgm.getOutgoingDisjunctiveEdge(v_opt.value()).value();
    EXPECT_EQ(dgm.transmission_graph[transmission_e].weight, 155613);
}
