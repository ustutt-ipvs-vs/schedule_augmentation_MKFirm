#pragma once

#include "inputLoader.h"
#include <filesystem>
#include <string>
#include <vector>

namespace io {

class ProgramOptions {
public:
  explicit ProgramOptions(std::vector<std::string> &arguments);

  [[nodiscard]] auto getTopologyPath() const -> FilePath;

  [[nodiscard]] auto getTimeTriggeredStreamsPath() const -> FilePath;

  [[nodiscard]] auto getSchedulePath() const -> FilePath;

  [[nodiscard]] auto getEmergencyStreams() const -> FilePath;

  [[nodiscard]] auto getOutputPath() const -> FilePath;

private:
  std::string topology_path_;
  std::string tt_streams_path_;
  std::string emergency_streams_path_;
  std::string schedule_path_;
  std::string output_path_ = "./sample_output.json";
};

} // namespace io
