#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "inputLoader.h"
namespace io
{
    class ProgramOptions
    {
    public:
        explicit ProgramOptions(std::vector<std::string> &arguments);

        [[nodiscard]] auto getTopologyPath() const -> FilePath;

        [[nodiscard]] auto getTimeTriggeredStreamsPath() const -> FilePath;

        [[nodiscard]] auto getSchedulePath() const -> FilePath;

        [[nodiscard]] auto getEmergencyStreams() const -> FilePath;


    private:
        std::string topology_path_;
        std::string tt_streams_path_;
        std::string emergency_streams_path_;
        std::string schedule_path_;
    };
} // namespace io
