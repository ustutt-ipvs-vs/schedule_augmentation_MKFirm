#include "inputLoader.h"

#include <fstream>
#include <iostream>
#include <src/network/message_stream.h>

#include "../util/constants.h"

auto io::load_emergency_traffic(const FilePath &in) -> std::vector<tsndgm::EmergencyStream>
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

        std::vector<tsndgm::EmergencyStream> streams;
        streams.reserve(j.size());
        for (const auto &js : j)
        {
            streams.emplace_back(tsndgm::createEmergencyStream(js));
        }

        return streams;
    }
    catch (nlohmann::json::parse_error &e)
    {
        std::cout << "Error parsing json file: " << in.string() << "\n";
        std::cout << e.what() << std::endl;
        std::exit(error_codes::JSON_PARSING_FAILED);
    }
}


auto io::load_time_triggered_traffic(const FilePath &in, const std::shared_ptr<tsndgm::NetworkTopology> &network)
    -> std::vector<tsndgm::MessageStream>
{
    try
    {
        std::ifstream i(in);
        nlohmann::json j = nlohmann::json::parse(i);

        std::vector<tsndgm::MessageStream> streams;
        streams.reserve(j.size());
        for (const auto &js : j)
        {
            streams.emplace_back(tsndgm::createMessageStream(js));
        }

        return streams;
    }
    catch (nlohmann::json::parse_error &e)
    {
        std::cout << "Error parsing json file: " << in.string() << "\n";
        std::cout << e.what() << std::endl;
        std::exit(3);
    }
}

auto io::load_schedule(const std::filesystem::path &in) -> std::vector<tsndgm::StreamSchedule>
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

auto io::set_routes(const std::vector<tsndgm::StreamSchedule> &schedules, std::vector<tsndgm::MessageStream> &streams)
    -> void
{
    std::unordered_map<tsndgm::StreamID, tsndgm::PathRoute> route_map;

    for (const auto &current_stream : schedules)
    {
        route_map[current_stream.stream_id] = tsndgm::build_route(current_stream);
    }
    for (tsndgm::MessageStream &current_stream : streams)
    {
        current_stream.route.route = route_map.at(current_stream.id);
    }
}
