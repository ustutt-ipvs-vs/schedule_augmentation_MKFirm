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
