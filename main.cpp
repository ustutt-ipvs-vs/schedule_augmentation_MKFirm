#include <memory>

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
    std::filesystem::__cxx11::path network_file = "../data/network.json";
    const auto network = std::make_shared<tsndgm::NetworkTopology>(network_file);
    network->print_topology();
}
