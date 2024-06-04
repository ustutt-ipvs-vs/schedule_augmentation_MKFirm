#ifndef TSN_DGM_TOPOLOGY_H
#define TSN_DGM_TOPOLOGY_H

#include <boost/graph/adjacency_list.hpp>
#include <filesystem>

namespace tsndgm {

typedef unsigned int DeviceId;
typedef long Tick;
typedef Tick Delay;
typedef unsigned long DataRate;

#define DEFAULT_DATA_RATE 12500000UL                            // 100Mbps
#define SECONDS_TO_TICKS(seconds) (Tick) seconds * 1000000000UL // 1 Tick = 1ns

static DeviceId unique_id = 0;
struct NetworkDeviceProperty {
  DeviceId id;
  Delay processing_delay;
  std::string name;

  NetworkDeviceProperty() {}

  explicit NetworkDeviceProperty(DeviceId id, Delay processing_delay = 0,
                                 std::string name = "")
      : id(id), processing_delay(processing_delay), name(name) {
    unique_id = std::max(id, unique_id) + 1;
  }
};

static std::ostream &operator<<(std::ostream &out,
                                const NetworkDeviceProperty &dev) {
  if (dev.name == "")
    return out << dev.id;
  return out << dev.name;
}

static NetworkDeviceProperty
UniqueNetworkDeviceProperty(Delay processing_delay = 0) {
  return NetworkDeviceProperty(unique_id, processing_delay);
}

enum DataLinkType { wired, wireless };

/** DataLinkProperty has different semantics for wireline and wireless links.
 * For wireline links, type = wired, multiple_subcarriers = false, and the
 * frame's delay is computed via
 *  d = frame_size / data_rate + propagation_delay
 * For wireless links, type = wireless, multiple_subcarriers \in {true, false},
 * and the frames's delay is computed via
 *  d = frame_size / data_rate + propagation_delay + RTI
 * In particular, the data_rate now specifies a "wireline" transmission delay
 * that covers the wired portion between the TSN bridge's egress port and the
 * radio link. The delay of the radio link itself is covered by the RTI. */
struct DataLinkProperty {
  DataLinkType type;
  DataRate data_rate;
  Delay propagation_delay;
  bool multiple_subcarriers;

  DataLinkProperty(DataLinkType type = wired,
                   DataRate data_rate = DEFAULT_DATA_RATE,
                   Delay propagation_delay = 0,
                   bool multiple_subcarriers = false)
      : type(type), data_rate(data_rate), propagation_delay(propagation_delay),
        multiple_subcarriers(multiple_subcarriers) {}

  DataLinkProperty(DataLinkType type, bool multiple_subcarriers)
      : type(type), data_rate(DEFAULT_DATA_RATE), propagation_delay(0),
        multiple_subcarriers(multiple_subcarriers) {}
};

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS,
                              NetworkDeviceProperty, DataLinkProperty>
    network_topology_t;

typedef std::pair<DeviceId, DeviceId> Edge;
typedef std::pair<Edge, DataLinkProperty> DataLink;

class NetworkTopology {
public:
  typedef boost::graph_traits<network_topology_t>::vertex_descriptor V;
  typedef boost::graph_traits<network_topology_t>::edge_descriptor E;

  NetworkTopology() {
    NetworkTopology(std::initializer_list<NetworkDeviceProperty>{},
                    std::initializer_list<DataLink>{});
  }
  NetworkTopology(std::filesystem::path in) {
    NetworkTopology(std::initializer_list<NetworkDeviceProperty>{},
                    std::initializer_list<DataLink>{});
    load_topology(in);
  }
  NetworkTopology(const std::vector<NetworkDeviceProperty> &device_properties,
                  const std::vector<DataLink> &data_links);

  void add_device(const NetworkDeviceProperty &device_property);
  void add_device(const NetworkDeviceProperty &device_property,
                  const std::vector<DataLink> &data_links);
  void add_data_link(const DataLink &data_link);
  void add_data_links(const std::vector<DataLink> &data_links);

  const NetworkDeviceProperty &get_device_property(DeviceId id) const;
  const DataLinkProperty &get_data_link_property(Edge edge) const;
  bool has_multiple_subcarriers(Edge edge) const;

  void remove_device(const DeviceId &device);
  void remove_devices(const std::vector<DeviceId> &devices);
  void remove_data_link(const Edge &edge);
  void remove_data_link(const DataLink &data_link);
  void remove_data_links(const std::vector<DataLink> &data_links);

  bool exists(DeviceId id) const;
  bool exists(Edge edge) const;

  void clear();

  void print_topology();

  void dump_topology(std::filesystem::path out);
  void load_topology(std::filesystem::path in);

  inline const NetworkDeviceProperty &operator[](DeviceId v) const {
    return g[v];
  }

private:
  template <bool throw_error> V get_vertex_by_id(DeviceId id) const;
  E get_edge_by_ids(Edge edge) const;

  V get_vertex_by_id(
      const std::vector<NetworkDeviceProperty> &device_properties, DeviceId id);
  std::pair<V, V>
  get_edge_by_ids(const std::vector<NetworkDeviceProperty> &device_properties,
                  Edge edge);

  network_topology_t g;
};

} // namespace tsndgm

#endif // TSN_DGM_TOPOLOGY_H
