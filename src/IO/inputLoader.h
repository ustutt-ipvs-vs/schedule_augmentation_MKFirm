#pragma once

#include <filesystem>
#include <vector>
#include "../network/emergency_stream.h"

/**
 * collects functions to load the input files (json), extracts their content and
 * calls the appropriate constructors to create the objects
 */
namespace io
{

    typedef std::filesystem::__cxx11::path FilePath;

    auto load_emergency_traffic(const FilePath &in) -> std::vector<tsndgm::EmergencyStream>;


} // namespace io
