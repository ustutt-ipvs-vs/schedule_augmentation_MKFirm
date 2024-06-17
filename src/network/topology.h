#ifndef TSN_DGM_TOPOLOGY_H
#define TSN_DGM_TOPOLOGY_H

#include <boost/graph/adjacency_list.hpp>
#include <filesystem>
#include <utility>
#include "../util/typedefs.h"
#include "emergency_stream.h"

namespace tsndgm
{

    static DeviceId unique_id = 0;

    struct NetworkDeviceProperty
    {
        DeviceId id;
        Delay processing_delay;
        std::string name;
        unsigned int queues_per_port;

        NetworkDeviceProperty() = default;

        explicit NetworkDeviceProperty(const DeviceId id, const Delay processing_delay = 0, std::string name = "",
                                       const unsigned int queues_per_port = 8) :
            id(id), processing_delay(processing_delay), name(std::move(name)), queues_per_port(queues_per_port)
        {
            // TODO should this be "unique_id = max(id + 1, unique_id)"? Otherwise we
            // increase the unique_id even if the id is smaller than the current
            // unique_id
            unique_id = std::max(id, unique_id) + 1;
        }
    };

    static std::ostream &operator<<(std::ostream &out, const NetworkDeviceProperty &dev)
    {
        if (dev.name.empty())
            return out << dev.id;
        return out << dev.name;
    }

    static NetworkDeviceProperty UniqueNetworkDeviceProperty(Delay processing_delay = 0)
    {
        return NetworkDeviceProperty(unique_id, processing_delay);
    }

    struct DataLinkProperty
    {
        DataRate data_rate;
        Delay propagation_delay;
        BurstSize aggregated_emergency_burst_size = 0;
        DataRate aggregated_emergency_refill_rate = 0;

        explicit DataLinkProperty(const DataRate data_rate = mbps_to_DataRate(100), const Delay propagation_delay = 0) :
            data_rate(data_rate), propagation_delay(propagation_delay)
        {
        }
    };

    typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS, NetworkDeviceProperty,
                                  DataLinkProperty>
        network_topology_t;

    typedef std::pair<Edge, DataLinkProperty> DataLink;

    class NetworkTopology
    {
    public:
        typedef boost::graph_traits<network_topology_t>::vertex_descriptor V;
        typedef boost::graph_traits<network_topology_t>::edge_descriptor E;

        NetworkTopology() = default;
        //{
        //    NetworkTopology(std::initializer_list<NetworkDeviceProperty>{}, std::initializer_list<DataLink>{});
        //}

        NetworkTopology(const std::vector<NetworkDeviceProperty> &device_properties,
                        const std::vector<DataLink> &data_links);

        void add_device(const NetworkDeviceProperty &device_property);
        void add_device(const NetworkDeviceProperty &device_property, const std::vector<DataLink> &data_links);
        void add_data_link(const DataLink &data_link);
        void add_data_links(const std::vector<DataLink> &data_links);

        [[nodiscard]] const NetworkDeviceProperty &get_device_property(DeviceId id) const;
        [[nodiscard]] const DataLinkProperty &get_data_link_property(const Edge &edge) const;

        void remove_device(const DeviceId &device);
        void remove_devices(const std::vector<DeviceId> &devices);
        void remove_data_link(const Edge &edge);
        void remove_data_link(const DataLink &data_link);
        void remove_data_links(const std::vector<DataLink> &data_links);

        [[nodiscard]] bool exists(DeviceId id) const;
        [[nodiscard]] bool exists(const Edge &edge) const;

        void update_aggregated_emergency_usage_of_edge(Edge &edge, BurstSize new_aggregated_burst_size, DataRate new_aggregated_data_rate);
        auto calculate_aggregated_emergency_usage(std::vector<EmergencyStream> &emergency_streams) -> void;
        void clear();

        void print_topology() const;

        void dump_topology(const std::filesystem::path &out);

        const NetworkDeviceProperty &operator[](DeviceId v) const { return g[v]; }

    private:
        template <bool throw_error>
        [[nodiscard]] V get_vertex_by_id(DeviceId id) const;
        [[nodiscard]] E get_edge_by_ids(const Edge &edge) const;

        V get_vertex_by_id(const std::vector<NetworkDeviceProperty> &device_properties, DeviceId id);
        auto get_edge_by_ids(const std::vector<NetworkDeviceProperty> &device_properties, const Edge &edge)
            -> std::pair<V, V>;

        network_topology_t g;
    };
} // namespace tsndgm

#endif // TSN_DGM_TOPOLOGY_H
