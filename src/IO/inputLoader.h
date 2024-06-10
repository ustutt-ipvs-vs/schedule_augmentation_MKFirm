#pragma once

#include <filesystem>
#include <vector>
#include "../network/emergency_stream.h"

namespace tsndgm
{
    class MessageStream;
}
/**
 * collects functions to load the input files (json), extracts their content and
 * calls the appropriate constructors to create the objects
 */
namespace io
{

    typedef std::filesystem::path FilePath;

    auto load_emergency_traffic(const FilePath &in) -> std::vector<tsndgm::EmergencyStream>;

    auto load_time_triggered_traffic(const FilePath &in, const std::shared_ptr<tsndgm::NetworkTopology> &network)
        -> std::vector<tsndgm::MessageStream>;

    auto load_schedule(const FilePath &in) -> std::vector<tsndgm::StreamSchedule>;

    auto set_routes(const std::vector<tsndgm::StreamSchedule> &schedules, std::vector<tsndgm::MessageStream> &streams)
        -> void;

} // namespace io
