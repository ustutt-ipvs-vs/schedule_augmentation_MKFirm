#include "message_stream.h"

namespace tsndgm
{
    void MessageStream::compute_wired_rtis()
    {
        for (const auto edge : route->route)
        // for (const TreeRouteHop &hop : *route)
        {
            auto &data_link_property = network->get_data_link_property(edge);

            auto &device_property = network->get_device_property(edge.second);
            rti_map[edge] = RTI(frame_size, data_link_property.data_rate, data_link_property.propagation_delay,
                                device_property.processing_delay);
        }
    }

    void MessageStream::initialize() { compute_wired_rtis(); }

    nlohmann::json MessageStream::dump() const
    {
        nlohmann::json j = {
            {"route", {}},   {"period", period}, {"frame_size", frame_size}, {"e2e_latency", e2e_latency},
            {"rti_map", {}}, {"phase", phase},   {"jitter", jitter},         {"name", name},
        };

        for (const auto hop : route->route)
            j["route"].push_back(hop);

        return j;
    }

    MessageStream::MessageStream(const std::shared_ptr<NetworkTopology> &network, nlohmann::json j)
    {
        PathRoute path;
        for (auto e : j["route"])
            path.push_back(Edge(e[0], e[1]));

        std::shared_ptr<Route> route = make_shared<Route>(std::move(path));

        RTIMap rti_map;
        if (!j["rti_map"].is_null())
        {
            for (auto edge_rti : j["rti_map"])
            {
                Edge e = Edge(edge_rti["edge"][0], edge_rti["edge"][1]);

                if (!edge_rti["rti"].is_null())
                {
                    RTI rti(static_cast<Delay>(edge_rti["rti"]["d_trans_max"]), edge_rti["rti"]["d_trans_min"],
                            edge_rti["rti"]["d_prop+d_proc"]);
                    rti_map[e] = rti;
                }
                else
                {
                    throw std::logic_error("Invalid stream file: 'rti_map' entry container either "
                                           "'rti', or {'reliability', 'histogram'}");
                }
            }
        }

        *this = MessageStream(network, route, j["period"], j["frame_size"], j["e2e_latency"], rti_map, j["phase"],
                              j["jitter"], j["name"]);
    }

    void dump_streams(const std::vector<MessageStream> &streams, std::filesystem::path out)
    {
        nlohmann::json j = {};
        for (const MessageStream &stream : streams)
        {
            j.push_back(stream.dump());
        }

        std::ofstream o(out);
        o << std::setw(4) << j << std::endl;
    }

    std::vector<MessageStream> load_streams(const std::shared_ptr<NetworkTopology> &network,
                                            const std::filesystem::path &in)
    {
        try
        {
            std::ifstream i(in);
            nlohmann::json j = nlohmann::json::parse(i);

            std::vector<MessageStream> streams;
            streams.reserve(j.size());
            for (const auto &js : j)
            {
                streams.emplace_back(network, js);
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
} // namespace tsndgm
