#include "scheduleLoader.h"

#include <src/util/constants.h>

namespace io
{

    std::vector<tsndgm::StreamSchedule> load_schedule(const std::filesystem::path &in)
    {
        try
        {
            std::ifstream i(in);
            if (not i.good())
            {
                std::cout << "Error opening file: " << in.string() << "\n";
                std::exit(error_codes::FILE_NOT_FOUND);
            }
            nlohmann::json j = nlohmann::json::parse(i);

            std::vector<tsndgm::StreamSchedule> scheduled_streams;
            scheduled_streams.reserve(j.size());
            for (const auto &js : j)
            {
                scheduled_streams.emplace_back(tsndgm::createStreamSchedule(js));
            }
            return scheduled_streams;
        }
        catch (nlohmann::json::parse_error &e)
        {
            std::cout << "Error parsing json file: " << in.string() << "\n";
            std::cout << e.what() << std::endl;
            std::exit(error_codes::JSON_PARSING_FAILED);
        }
    }

    void set_routes(const std::vector<tsndgm::StreamSchedule> &schedules, std::vector<tsndgm::MessageStream> &streams)
    {
        std::unordered_map<tsndgm::StreamID, tsndgm::PathRoute> route_map;

        for (const auto &current_stream : schedules)
        {
            route_map[current_stream.stream_id] = build_route(current_stream);
        }
        for (tsndgm::MessageStream &current_stream : streams)
        {
            current_stream.route->route = route_map.at(current_stream.id);
        }
    }


    auto build_route(const tsndgm::StreamSchedule &stream) -> tsndgm::PathRoute
    {
        const auto &frame = stream.frames.front();
        tsndgm::PathRoute route;
        for (const auto &frame_transmission : frame.transmissions)
        {
            route.emplace_back(frame_transmission.source, frame_transmission.target);
        }
        return route;
    }
} // namespace io
