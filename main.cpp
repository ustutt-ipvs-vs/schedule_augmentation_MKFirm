#include <memory>

#include "src/IO/programOptions.h"
#include "src/IO/scheduleLoader.h"
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
            return io::ProgramOptions(arguments);
        }
        catch (const std::runtime_error &e)
        {
            std::cout << e.what();
            std::exit(1);
        }
    }();

    const auto network = std::make_shared<tsndgm::NetworkTopology>(options.getTopologyPath());
    network->print_topology();
    std::cout << "Loaded network topology!\n";

    const auto streams = load_streams(network, options.getTimeTriggeredStreamsPath());

    const auto emergency_streams = io::load_emergency_traffic(options.getEmergencyStreams(), *network);
    std::cout << "Loaded streams!\n";

    const std::vector<tsndgm::StreamSchedule> scheduled_streams = tsndgm::load_schedule(options.getSchedulePath());
    std::cout << "Loaded schedule!\n";
    for(tsndgm::StreamSchedule i : scheduled_streams){
        std::cout << i.toString() << "\n";
    }

    tsndgm::DisjunctiveGraphModel dgm(network, streams);
    dgm.print();
    dgm.print_critical_path(tsndgm::CriticalPath::makespan);
}
