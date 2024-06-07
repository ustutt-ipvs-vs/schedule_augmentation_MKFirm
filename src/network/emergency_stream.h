#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include "route.h"
#include "../util/typedefs.h"

namespace tsndgm
{
    struct EmergencyStream
    {
        std::string name;
        BurstSize burst_size_byte;
        DataRate refill_rate_mbps;


        std::shared_ptr<Route> route;

        [[nodiscard]] auto dump() const -> nlohmann::json
        {
            nlohmann::json j = {
                {"id", name}, {"burst size", burst_size_byte}, {"data rate", refill_rate_mbps}, {"route", {}}};

            for (const auto &hop : route->route)
            {
                j["route"].push_back(hop);
            }
            return j;
        }
    };
} // namespace tsndgm
