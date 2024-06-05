#pragma once

#include <iostream>
#include "topology.h"

namespace tsndgm
{

    typedef std::vector<Edge> PathRoute;

    struct Route
    {
        DeviceId source;
        DeviceId destination;

        PathRoute route;

        explicit Route(PathRoute &&input_route)
        {
            route = std::move(input_route);
            source = route.front().first;
            destination = route.back().second;
        }

        auto print_route() const -> void
        {
            std::cout << "Route from " << source << " to " << destination << ": ";
            for (const auto &[from, to] : route)
            {
                std::cout << from << " -> " << to << " -> ";
            }
            std::cout << std::endl;
        }

        auto get_talker() -> Edge { return route.front(); }

        auto get_listeners() -> std::vector<Edge> { return {route.back()}; }
    };

} // namespace tsndgm

