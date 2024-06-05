#include "programOptions.h"
#include <CLI/CLI.hpp>


io::ProgramOptions::ProgramOptions(std::vector<std::string> &arguments)
{
    CLI::App app{"Dynamic Flow Scheduler v2"};

    app.add_option("-t,--topology", topology_path_, "File path to the network graph")->required();

    app.add_option("-s,--tt_streams", tt_streams_path_, "File path to the time-triggered streams")->required();

    app.add_option("-e,--e_streams", emergency_streams_path_, "File path to the emergency streams")->required();

    app.add_option("-z,--schedule", schedule_path_, "File path to the schedule of the TT streams")->required();

    // additional arguments can be added here.

    std::ranges::reverse(arguments);
    try
    {
        app.parse(arguments);
    }
    catch (const CLI::CallForHelp &e)
    {
        throw std::runtime_error(app.help()); // return help text
    }
    catch (const CLI::ParseError &e)
    {
        const auto errorMessage = std::string("Error parsing command-line arguments: ") + e.what();
        throw std::runtime_error(errorMessage);
    }
}
auto io::ProgramOptions::getTopologyPath() const -> FilePath { return topology_path_; }

auto io::ProgramOptions::getTimeTriggeredStreamsPath() const -> FilePath { return tt_streams_path_; }

auto io::ProgramOptions::getSchedulePath() const -> FilePath { return schedule_path_; }

auto io::ProgramOptions::getEmergencyStreams() const -> FilePath { return emergency_streams_path_; }
