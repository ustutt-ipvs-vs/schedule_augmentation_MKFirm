#include <boost/graph/graph_utility.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

#include "topology.h"

namespace tsndgm
{
    typedef boost::graph_traits<network_topology_t>::vertex_descriptor V;
    typedef boost::graph_traits<network_topology_t>::edge_descriptor E;

    NetworkTopology::NetworkTopology(const std::vector<NetworkDeviceProperty> &device_properties,
                                     const std::vector<DataLink> &data_links)
    {
        std::vector<std::pair<V, V>> edges;
        std::vector<DataLinkProperty> data_link_properties;

        for (const DataLink &data_link : data_links)
        {
            auto edge = get_edge_by_ids(device_properties, data_link.first);
            edges.push_back(std::move(edge));
            data_link_properties.push_back(data_link.second);
        }

        g = network_topology_t(edges.begin(), edges.end(), data_link_properties.data(), device_properties.size());

        for (auto vd : boost::make_iterator_range(boost::vertices(g)))
        {
            g[vd].id = device_properties[vd].id;
            g[vd].processing_delay = device_properties[vd].processing_delay;
            g[vd].name = device_properties[vd].name;
            g[vd].queues_per_port = device_properties[vd].queues_per_port;
        }
    }

    void NetworkTopology::add_device(const NetworkDeviceProperty &device_property)
    {
        auto [begin, end] = boost::vertices(g);
        if (std::find_if(begin, end, [&](auto v) { return g[v].id == device_property.id; }) == end)
        {
            V device = boost::add_vertex(g);
            g[device].id = device_property.id;
            g[device].processing_delay = device_property.processing_delay;
            g[device].name = device_property.name;
            g[device].queues_per_port = device_property.queues_per_port;
        }
    }

    void NetworkTopology::add_device(const NetworkDeviceProperty &device_property,
                                     const std::vector<DataLink> &data_links)
    {
        add_device(device_property);
        add_data_links(data_links);
    }

    void NetworkTopology::add_data_link(const DataLink &data_link)
    {
        Edge edge = data_link.first;
        V v1 = get_vertex_by_id<true>(edge.first);
        V v2 = get_vertex_by_id<true>(edge.second);
        if (!boost::edge(v1, v2, g).second)
        {
            boost::add_edge(v1, v2, data_link.second, g);
        }
    }

    void NetworkTopology::add_data_links(const std::vector<DataLink> &data_links)
    {
        for (const DataLink &data_link : data_links)
        {
            add_data_link(data_link);
        }
    }

    const NetworkDeviceProperty &NetworkTopology::get_device_property(DeviceId id) const
    {
        V v = get_vertex_by_id<true>(id);
        return boost::get(boost::vertex_bundle, g)[v];
    }

    const DataLinkProperty &NetworkTopology::get_data_link_property(Edge edge) const
    {
        E e = get_edge_by_ids(edge);
        return boost::get(boost::edge_bundle, g)[e];
    }

    void NetworkTopology::remove_device(const DeviceId &device)
    {
        V v = get_vertex_by_id<true>(device);
        boost::clear_vertex(v, g);
        boost::remove_vertex(v, g);
    }

    void NetworkTopology::remove_devices(const std::vector<DeviceId> &devices)
    {
        for (const DeviceId &device : devices)
            remove_device(device);
    }

    void NetworkTopology::remove_data_link(const Edge &edge)
    {
        E e = get_edge_by_ids(edge);
        remove_edge(e, g);
    }

    void NetworkTopology::remove_data_link(const DataLink &data_link) { remove_data_link(data_link.first); }

    void NetworkTopology::remove_data_links(const std::vector<DataLink> &data_links)
    {
        for (const DataLink &data_link : data_links)
        {
            remove_data_link(data_link);
        }
    }

    bool NetworkTopology::exists(DeviceId id) const
    {
        V v = get_vertex_by_id<false>(id);
        return v == g.null_vertex();
    }

    bool NetworkTopology::exists(Edge edge) const
    {
        V v1 = get_vertex_by_id<false>(edge.first);
        V v2 = get_vertex_by_id<false>(edge.second);

        E e;
        bool found;
        boost::tie(e, found) = boost::edge(v1, v2, g);

        return found;
    }

    void NetworkTopology::clear()
    {
        boost::graph_traits<network_topology_t>::vertex_iterator vi, vi_end, next;
        boost::tie(vi, vi_end) = vertices(g);
        for (next = vi; vi != vi_end; vi = next)
        {
            ++next;
            clear_vertex(*vi, g);
            remove_vertex(*vi, g);
        }
    }

    void NetworkTopology::print_topology() const
    {
        std::cout << "Topology" << std::endl;
        typename boost::graph_traits<network_topology_t>::out_edge_iterator out_i, out_end;
        V t;
        for (auto vd : boost::make_iterator_range(boost::vertices(g)))
        {
            std::cout << g[vd].id << " --> ";
            for (boost::tie(out_i, out_end) = boost::out_edges(vd, g); out_i != out_end; out_i++)
            {
                t = target(*out_i, g);
                std::cout << g[t].id << " ";
            }
            std::cout << std::endl;
        }

        std::cout << "ID\tName\tProcessing Delay\tQueues per Port" << std::endl;
        for (auto vd : boost::make_iterator_range(boost::vertices(g)))
        {
            std::cout << g[vd].id << "\t" << g[vd].name << "\t" << g[vd].processing_delay << "\t\t\t"
                      << g[vd].queues_per_port << std::endl;
        }
        std::cout << "Edge\tData Rate\tPropagation Delay" << std::endl;
        for (auto ed : boost::make_iterator_range(boost::edges(g)))
        {
            std::cout << "(" << g[source(ed, g)].id << ", " << g[target(ed, g)].id << ")\t" << g[ed].data_rate << "\t\t"
                      << g[ed].propagation_delay << std::endl;
        }
    }

    void NetworkTopology::dump_topology(std::filesystem::path out)
    {
        nlohmann::json j = {{"nodes", nlohmann::json::array()}, {"links", nlohmann::json::array()}};

        for (auto vd : boost::make_iterator_range(boost::vertices(g)))
        {
            j["nodes"].push_back(
                {{"id", g[vd].id}, {"processing_delay_ns", g[vd].processing_delay}, {"name", g[vd].name}});
        }

        for (auto ed : boost::make_iterator_range(boost::edges(g)))
        {
            j["links"].push_back({{"source", boost::source(ed, g)},
                                  {"target", boost::target(ed, g)},
                                  {"data_rate", g[ed].data_rate},
                                  {"propagation_delay", g[ed].propagation_delay}});
        }

        std::ofstream o(out);
        o << std::setw(4) << j << std::endl;
    }

    void NetworkTopology::load_topology(const std::filesystem::path &in)
    {
        try
        {
            std::ifstream i(in);
            nlohmann::json j = nlohmann::json::parse(i);
            for (auto n : j["nodes"])
            {
                NetworkDeviceProperty device(n["id"], n["processing_delay_ns"], n["name"], n["queues_per_port"]);
                add_device(device);
            }

            for (auto l : j["links"])
            {
                Edge edge(l["source"], l["target"]);
                DataLinkProperty link =
                    DataLinkProperty{mbps_to_DataRate(l["link_speed_mbps"]), l["propagation_delay_ns"]};
                add_data_link({edge, link});
            }
        }
        catch (nlohmann::json::parse_error &e)
        {
            std::cout << "Error parsing json file: " << in.string() << "\n";
            std::cout << e.what() << std::endl;
            std::exit(2);
        }
    }

    template <bool throw_error>
    V NetworkTopology::get_vertex_by_id(DeviceId id) const
    {
        for (auto vd : boost::make_iterator_range(boost::vertices(g)))
        {
            if (g[vd].id == id)
                return vd;
        }
        if (throw_error)
            throw std::runtime_error{"device not found: id " + std::to_string(id)};
        return g.null_vertex();
    }

    V NetworkTopology::get_vertex_by_id(const std::vector<NetworkDeviceProperty> &device_properties, DeviceId id)
    {
        int i = 0;
        for (const NetworkDeviceProperty &device_property : device_properties)
        {
            if (device_property.id == id)
                return i;
            i++;
        }
        throw std::runtime_error{"device not found: id " + std::to_string(id)};
    }

    E NetworkTopology::get_edge_by_ids(Edge edge) const
    {
        V v1 = get_vertex_by_id<true>(edge.first);
        V v2 = get_vertex_by_id<true>(edge.second);

        E e;
        bool found;
        boost::tie(e, found) = boost::edge(v1, v2, g);
        if (found)
            return e;

        throw std::runtime_error{"edge not found: (" + std::to_string(v1) + ", " + std::to_string(v2) + ")"};
    }

    std::pair<V, V> NetworkTopology::get_edge_by_ids(const std::vector<NetworkDeviceProperty> &device_properties,
                                                     Edge edge)
    {
        V v1 = get_vertex_by_id(device_properties, edge.first);
        V v2 = get_vertex_by_id(device_properties, edge.second);
        return std::make_pair(v1, v2);
    }
} // namespace tsndgm
