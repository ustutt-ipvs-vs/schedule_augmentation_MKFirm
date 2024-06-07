#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include "../network/message_stream.h"
#include "../network/schedule.h"


namespace io
{

    auto load_schedule(const std::filesystem::path &in) -> std::vector<tsndgm::StreamSchedule>;

    auto set_routes(const std::vector<tsndgm::StreamSchedule> &schedules, std::vector<tsndgm::MessageStream> &streams)
        -> void;

    /**
     * Create the PathRoute of the given stream
     * @param stream in StreamSchedule format
     * @return
     */
    auto build_route(const tsndgm::StreamSchedule &stream) -> tsndgm::PathRoute;

} // namespace io
