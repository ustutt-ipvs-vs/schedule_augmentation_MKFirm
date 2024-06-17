#pragma once

#include "schedule.h"


#include <iostream>
// "topology.h"

namespace tsndgm
{

    typedef std::vector<Edge> PathRoute;

    struct RouteWrapper
    {
        DeviceId source;
        DeviceId destination;

        PathRoute route;

        RouteWrapper()
        {
            // this constructor is needed so that Route is default constructable. Required to create a proper StreamMap.
            source = -1;
            destination = -1;
        }

        RouteWrapper(const DeviceId source, const DeviceId destination)
        {
            this->source = source;
            this->destination = destination;
        }

        explicit RouteWrapper(PathRoute &&input_route)
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

        [[nodiscard]] auto get_talker() const -> Edge { return route.front(); }

        [[nodiscard]] auto get_listeners() const -> std::vector<Edge> { return {route.back()}; }
    };

    /**
     * Create the PathRoute of the given stream
     * @param stream in StreamSchedule format
     * @return
     */
    inline auto build_route(const StreamSchedule &stream) -> PathRoute
    {
        const auto &frame = stream.frames.front();
        PathRoute route;
        for (const auto &frame_transmission : frame.transmissions)
        {
            route.emplace_back(frame_transmission.source, frame_transmission.target);
        }
        return route;
    }

} // namespace tsndgm
