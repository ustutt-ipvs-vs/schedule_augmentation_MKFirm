#include "inputLoader.h"

#include <fstream>

auto io::load_emergency_traffic(const FilePath &in, const tsndgm::NetworkTopology &topology)
    -> std::vector<tsndgm::EmergencyStream>
{
    try
    {
        std::ifstream i(in);
        nlohmann::json j = nlohmann::json::parse(i);

        std::vector<tsndgm::EmergencyStream> streams;
        streams.reserve(j.size());
        for (const auto &js : j)
        {
            auto temp_stream =
                tsndgm::EmergencyStream{.name = js["id"], .burst_size_byte = js["burst size"], .refill_rate_mbps = js["data rate"]};

            // TODO we need a way to map the network link name to an Edge here
            /*
            for (const auto& hop : js["route"])
            {
                tsndgm::Edge edge = topology. ;
                temp_stream.route->route.emplace_back(edge);
            }
            */
            streams.emplace_back(temp_stream);
        }

        return streams;
    }
    catch (nlohmann::json::parse_error &e)
    {
        std::cout << "Error parsing json file: " << in.string() << "\n";
        std::cout << e.what() << std::endl;
        std::exit(4);
    }
}
