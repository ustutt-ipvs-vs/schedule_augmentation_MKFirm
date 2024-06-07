#include "inputLoader.h"

#include <fstream>
#include <iostream>
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
            auto temp_stream = tsndgm::EmergencyStream{.id = js["streamID"],
                                                       .name = js["name"],
                                                       .source = js["source"],
                                                       .destination = js["target"],
                                                       .bucket_size_byte = js["bucket_size_byte"],
                                                       .refill_rate = tsndgm::mbps_to_DataRate(js["rate_mbps"])};
            tsndgm::PathRoute route;
            route.reserve(js["route"].size());
            for (const auto &hop : js["route"])
            {
                route.emplace_back(hop["from"], hop["to"]);
            }
            temp_stream.route = std::make_shared<tsndgm::Route>(std::move(route));

            streams.emplace_back(temp_stream);
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
