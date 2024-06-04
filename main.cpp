#include <memory>
#include <string>

#include "src/network/topology.h"

auto main(int argc, char* argv[])
    -> int
{
    /*
     * TODO:
     * - get command line arguments (CLI11?)
     * - parse network file
     * - parse streams file
     * - parse schedule file
     *
     * - build dgm graph from schedule
     * - update dgm edge weights
     * - check dgm graph for feasibility
     *
     * - create output
     */
    std::string network_file = "data/network.json";
    auto network = make_shared<tsndgm::NetworkTopology>(network_file);
}
