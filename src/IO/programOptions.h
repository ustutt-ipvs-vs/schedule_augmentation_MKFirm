#pragma once

#include <filesystem>
#include <string>
#include <vector>

class ProgramOptions
{
public:
    explicit ProgramOptions(std::vector<std::string> &arguments);

    [[nodiscard]] auto getTopologyPath() const -> std::filesystem::__cxx11::path;

    [[nodiscard]] auto getStreamsPath() const -> std::filesystem::__cxx11::path;

    [[nodiscard]] auto getSchedulePath() const -> std::filesystem::__cxx11::path;


private:
    std::string topology_path_;
    std::string streams_path_;
    std::string schedule_path_;
};
