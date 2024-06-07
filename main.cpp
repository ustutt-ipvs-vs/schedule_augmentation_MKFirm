#include <memory>

#include "src/IO/inputLoader.h"
#include "src/IO/programOptions.h"
#include "src/dgm/dgm.h"
#include "src/network/message_stream.h"
#include "src/network/topology.h"


auto main(const int argc, char *argv[]) -> int
{
    /*
     * TODO:
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

    auto streams = io::load_time_triggered_traffic(options.getTimeTriggeredStreamsPath(), network);
    std::cout << "Loaded TT-streams!\n";

    const auto emergency_streams = io::load_emergency_traffic(options.getEmergencyStreams());
    std::cout << "Loaded ET-streams!\n";
    for (const tsndgm::EmergencyStream &es : emergency_streams)
    {
        std::cout << es.to_string() << "\n";
    }

    const std::vector<tsndgm::StreamSchedule> scheduled_streams = io::load_schedule(options.getSchedulePath());
    std::cout << "Loaded schedule!\n";
    for (const tsndgm::StreamSchedule &ss : scheduled_streams)
    {
        std::cout << ss.toString() << "\n";
    }

    io::set_routes(scheduled_streams, streams);

    std::cout << "input loading and parsing done!\n";

    tsndgm::DisjunctiveGraphModel dgm(network, streams);
    dgm.print();
    // dgm.print_critical_path(tsndgm::CriticalPath::makespan);
}
