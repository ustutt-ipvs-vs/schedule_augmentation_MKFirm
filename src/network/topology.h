#ifndef TSN_DGM_TOPOLOGY_H
#define TSN_DGM_TOPOLOGY_H

#include <boost/graph/adjacency_list.hpp>

namespace tsndgm {

typedef unsigned int DeviceId;
typedef long Tick;
typedef Tick Delay;
typedef unsigned long DataRate;

#define DEFAULT_DATA_RATE 1000000000UL
#define SECONDS_TO_TICKS(seconds) (Tick) seconds * 1000000000UL

static DeviceId unique_id = 0;
struct NetworkDeviceProperty {
  DeviceId id;
  Delay processing_delay;

  NetworkDeviceProperty() {}

  explicit NetworkDeviceProperty(DeviceId id, Delay processing_delay = 0)
      : id(id), processing_delay(processing_delay) {
    unique_id = std::max(id, unique_id) + 1;
  }
};

static NetworkDeviceProperty
UniqueNetworkDeviceProperty(Delay processing_delay = 0) {
  return NetworkDeviceProperty(unique_id, processing_delay);
}

enum DataLinkType { wired, wireless };

struct DataLinkProperty {
  DataLinkType type;
  DataRate data_rate;
  Delay propagation_delay;

  DataLinkProperty(DataLinkType type = wired,
                   DataRate data_rate = DEFAULT_DATA_RATE,
                   Delay propagation_delay = 0)
      : type(type), data_rate(data_rate), propagation_delay(propagation_delay) {
  }
};

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS,
                              NetworkDeviceProperty, DataLinkProperty>
    network_topology_t;

typedef std::pair<DeviceId, DeviceId> Edge;
typedef std::pair<Edge, DataLinkProperty> DataLink;

class NetworkTopology {
public:
  NetworkTopology() : g() {}
  NetworkTopology(const std::vector<NetworkDeviceProperty> &device_properties,
                  const std::vector<DataLink> &data_links);

  void add_device(const NetworkDeviceProperty &device_property);
  void add_device(const NetworkDeviceProperty &device_property,
                  const std::vector<DataLink> &data_links);
  void add_data_link(const DataLink &data_link);
  void add_data_links(const std::vector<DataLink> &data_links);

  const NetworkDeviceProperty &get_device_property(DeviceId id) const;
  const DataLinkProperty &get_data_link_property(Edge edge) const;

  void remove_device(const DeviceId &device);
  void remove_devices(const std::vector<DeviceId> &devices);
  void remove_data_link(const Edge &edge);
  void remove_data_link(const DataLink &data_link);
  void remove_data_links(const std::vector<DataLink> &data_links);

  bool exists(DeviceId id) const;
  bool exists(Edge edge) const;

  void clear();

  void print_topology();

private:
  typedef boost::graph_traits<network_topology_t>::vertex_descriptor V;
  typedef boost::graph_traits<network_topology_t>::edge_descriptor E;

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
