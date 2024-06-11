#pragma once

#include <filesystem>
#include <vector>
#include "../network/emergency_stream.h"
#include "../network/message_stream.h"

/**
 * collects functions to load the input files (json), extracts their content and
 * calls the appropriate constructors to create the objects
 */
namespace io
{

    typedef std::filesystem::path FilePath;

    auto load_emergency_traffic(const FilePath &in) -> std::vector<tsndgm::EmergencyStream>;

    auto load_time_triggered_traffic(const FilePath &in) -> std::unordered_map<tsndgm::StreamID, tsndgm::MessageStream>;

    auto load_schedule(const FilePath &in) -> std::vector<tsndgm::StreamSchedule>;

    auto set_routes(const std::vector<tsndgm::StreamSchedule> &schedules,
                    std::unordered_map<tsndgm::StreamID, tsndgm::MessageStream> &streams) -> void;

    auto load_topology(const FilePath &in) -> tsndgm::NetworkTopology;

    auto check_file_loading(const std::ifstream &i, const FilePath &in) -> void;

} // namespace io
