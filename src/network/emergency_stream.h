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
        BurstSize bucket_size_byte;
        DataRate refill_rate;


        std::shared_ptr<Route> route;

        [[nodiscard]] auto dump() const -> nlohmann::json
        {
            nlohmann::json j = {{"id", id},
                                {"name", name},
                                {"source", source},
                                {"destination", destination},
                                {"burst size", bucket_size_byte},
                                {"refill rate", refill_rate},
                                {"route", {}}};

            for (const auto &hop : route->route)
            {
                j["route"].push_back(hop);
            }
            return j;
        }

        [[nodiscard]] auto to_string() const -> std::string
        {
            std::stringstream ss;
            ss << id << ": " << name << "\t";
            ss << source << "->" << destination << "\t";
            ss << "b: " << bucket_size_byte << " r: " << refill_rate;

            return ss.str();
        }
    };

} // namespace tsndgm
