#include "programOptions.h"
#include <CLI/CLI.hpp>


ProgramOptions::ProgramOptions(std::vector<std::string> &arguments)
{
    CLI::App app{"Dynamic Flow Scheduler v2"};

    app.add_option("-t,--topology", topology_path_, "File path to the network graph")->required();

    app.add_option("-s,--streams", streams_path_, "File path to the streams")->required();

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
auto ProgramOptions::getTopologyPath() const -> std::filesystem::__cxx11::path { return topology_path_; }

auto ProgramOptions::getStreamsPath() const -> std::filesystem::__cxx11::path { return streams_path_; }

auto ProgramOptions::getSchedulePath() const -> std::filesystem::__cxx11::path { return schedule_path_; }
