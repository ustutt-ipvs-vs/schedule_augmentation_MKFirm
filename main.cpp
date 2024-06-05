#include <memory>

#include "src/IO/programOptions.h"
#include "src/dgm/dgm.h"
#include "src/network/message_stream.h"
#include "src/network/topology.h"

auto main(const int argc, char *argv[]) -> int
{
    /*
     * TODO:
     * - parse schedule file
     *
     * - build dgm graph from schedule
     * - update dgm edge weights
     * - check dgm graph for feasibility
     *
     * - create output
     */
    std::vector<std::string> arguments(argv + 1, argv + argc);
    const auto options = [&]
    {
        try
        {
            return ProgramOptions(arguments);
        }
        catch (const std::runtime_error &e)
        {
            std::cout << e.what();
            std::exit(1);
        }
    }();

    const auto network = std::make_shared<tsndgm::NetworkTopology>(options.getTopologyPath());
    network->print_topology();

    const auto streams = load_streams(network, options.getStreamsPath());

    tsndgm::DisjunctiveGraphModel dgm(network, streams);
    dgm.print();
    dgm.print_critical_path(tsndgm::CriticalPath::makespan);
}
