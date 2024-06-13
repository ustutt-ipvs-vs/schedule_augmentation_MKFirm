#include <IO/inputLoader.h>
#include <dgm/dgm.h>
#include <gtest/gtest.h>
#include <util/constants.h>





// fixture class
class dgmTest : public testing::Test {
    protected:
    dgmTest() {
        // Code in this constructor will be executed before *each* test
        
        // For debugging, this should print the dgm before each test, if it is initialized correctly 
        dgm.print();

    }

    // Runs *once* before the tests to build the dgm transmission graph
    static void SetUpTestSuite(){
        auto network = io::load_topology("../../tests/test_data/small_topology.json");
        std::cout << "Loaded network topology!\n";

        auto streams = io::load_time_triggered_traffic("../../tests/test_data/small_streams.json");
        std::cout << "Loaded TT-streams!\n";

        const std::vector<tsndgm::StreamSchedule> scheduled_streams = io::load_schedule("../../tests/test_data/small_transmission_output.json");
        std::cout << "Loaded schedule!\n";

        // Does not seem to intialize the public variable dgm...
        tsndgm::DisjunctiveGraphModel dgm(network, streams, scheduled_streams);
        // Not working
        //dgm = tsndgm::DisjunctiveGraphModel(network, streams, scheduled_streams);
        //dgm = new tsndgm::DisjunctiveGraphModel(network, streams, scheduled_streams)

    }

    public:
    // Shared dgm object between all tests
    static tsndgm::DisjunctiveGraphModel dgm;
};

// According to https://github.com/google/googletest/blob/main/docs/advanced.md this should make dgm available to the tests.
// But it does not seem to work, maybe because it is not initialized above?
//tsndgm::DisjunctiveGraphModel dgmTest::dgm;

TEST_F(dgmTest, operationVerticesTest) {
    
}

TEST_F(dgmTest, conjunctiveEdgesTest) {

}

