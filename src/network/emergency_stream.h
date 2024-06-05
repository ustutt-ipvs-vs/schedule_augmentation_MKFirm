#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include "route.h"

namespace tsndgm
{
    // TODO consider moving all generic typedefs in a single dedicated file
    typedef unsigned long BurstSize;

    struct EmergencyStream
    {
        std::string name;
        BurstSize burst_size;
        DataRate data_rate;


        std::shared_ptr<Route> route;

        [[nodiscard]] auto dump() const -> nlohmann::json
        {
            nlohmann::json j = {{"id", name}, {"burst size", burst_size}, {"data rate", data_rate}, {"route", {}}};

            for (const auto &hop : route->route)
            {
                j["route"].push_back(hop);
            }
            return j;
        }
    };
} // namespace tsndgm
