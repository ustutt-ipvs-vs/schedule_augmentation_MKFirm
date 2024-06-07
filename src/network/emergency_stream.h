#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include "../util/typedefs.h"
#include "route.h"

namespace tsndgm
{
    struct EmergencyStream
    {
        StreamID id;
        std::string name;
        DeviceId source;
        DeviceId destination;
        BurstSize burst_size_byte;
        DataRate refill_rate_mbps;


        std::shared_ptr<Route> route;

        [[nodiscard]] auto dump() const -> nlohmann::json
        {
            nlohmann::json j = {{"id", id},
                                {"name", name},
                                {"source", source},
                                {"destination", destination},
                                {"burst size", burst_size_byte},
                                {"refill rate", refill_rate_mbps},
                                {"route", {}}};

            for (const auto &hop : route->route)
            {
                j["route"].push_back(hop);
            }
            return j;
        }
    };
} // namespace tsndgm
