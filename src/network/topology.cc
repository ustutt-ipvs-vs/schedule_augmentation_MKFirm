#include "topology.h"
#include <fstream>
#include <nlohmann/json.hpp>

namespace tsndgm {

typedef boost::graph_traits<network_topology_t>::vertex_descriptor V;
typedef boost::graph_traits<network_topology_t>::edge_descriptor E;

NetworkTopology::NetworkTopology(const std::vector<NetworkDeviceProperty> &device_properties,
                                 const std::vector<DataLink> &data_links) {
  std::vector<std::pair<V, V>> edges;
  std::vector<DataLinkProperty> data_link_properties;

  for (const DataLink &data_link : data_links) {
    auto edge = get_edge_by_ids(device_properties, data_link.first);
    edges.push_back(std::move(edge));
    data_link_properties.push_back(data_link.second);
  }

  g = network_topology_t(edges.begin(), edges.end(), data_link_properties.data(), device_properties.size());

  for (const auto vd : boost::make_iterator_range(vertices(g))) {
    g[vd].id = device_properties[vd].id;
    g[vd].processing_delay = device_properties[vd].processing_delay;
    g[vd].name = device_properties[vd].name;
    g[vd].queues_per_port = device_properties[vd].queues_per_port;
  }
}

void NetworkTopology::add_device(const NetworkDeviceProperty &device_property) {
  auto [begin, end] = vertices(g);
  if (std::find_if(begin, end, [&](auto v) { return g[v].id == device_property.id; }) == end) {
    const V device = boost::add_vertex(g);
    g[device].id = device_property.id;
    g[device].processing_delay = device_property.processing_delay;
    g[device].name = device_property.name;
    g[device].queues_per_port = device_property.queues_per_port;
  }
}

void NetworkTopology::add_device(const NetworkDeviceProperty &device_property,
                                 const std::vector<DataLink> &data_links) {
  add_device(device_property);
  add_data_links(data_links);
}

void NetworkTopology::add_data_link(const DataLink &data_link) {
  const Edge edge = data_link.first;
  const V v1 = get_vertex_by_id<true>(edge.first);
  const V v2 = get_vertex_by_id<true>(edge.second);
  if (!boost::edge(v1, v2, g).second) {
    boost::add_edge(v1, v2, data_link.second, g);
  }
}

void NetworkTopology::add_data_links(const std::vector<DataLink> &data_links) {
  for (const DataLink &data_link : data_links) {
    add_data_link(data_link);
  }
}

const NetworkDeviceProperty &NetworkTopology::get_device_property(const DeviceId id) const {
  const V v = get_vertex_by_id<true>(id);
  return get(boost::vertex_bundle, g)[v];
}

const DataLinkProperty &NetworkTopology::get_data_link_property(const Edge &edge) const {
  const E e = get_edge_by_ids(edge);
  return get(boost::edge_bundle, g)[e];
}

auto NetworkTopology::add_et_stream_to_aggregated_et_usage_of_edge(const Edge &edge,
                                                                   const EmergencyStream &stream) -> void {
  const E e = get_edge_by_ids(edge);
  DataLinkProperty &edge_property = get(boost::edge_bundle, g)[e];
  edge_property.aggregated_emergency_burst_size += stream.bucket_size_byte;
  edge_property.aggregated_emergency_refill_rate += stream.refill_rate;
}

void NetworkTopology::remove_device(const DeviceId &device) {
  const V v = get_vertex_by_id<true>(device);
  clear_vertex(v, g);
  remove_vertex(v, g);
}

void NetworkTopology::remove_devices(const std::vector<DeviceId> &devices) {
  for (const DeviceId &device : devices)
    remove_device(device);
}

void NetworkTopology::remove_data_link(const Edge &edge) {
  const E e = get_edge_by_ids(edge);
  remove_edge(e, g);
}

void NetworkTopology::remove_data_link(const DataLink &data_link) { remove_data_link(data_link.first); }

void NetworkTopology::remove_data_links(const std::vector<DataLink> &data_links) {
  for (const DataLink &data_link : data_links) {
    remove_data_link(data_link);
  }
}

bool NetworkTopology::exists(const DeviceId id) const {
  const V v = get_vertex_by_id<false>(id);
  return v != network_topology_t::null_vertex();
}

bool NetworkTopology::exists(const Edge &edge) const {
  const V v1 = get_vertex_by_id<false>(edge.first);
  const V v2 = get_vertex_by_id<false>(edge.second);

  auto [e, found] = boost::edge(v1, v2, g);

  return found;
}

auto NetworkTopology::update_all_edges_aggregated_et_usage(const std::vector<EmergencyStream> &emergency_streams)
    -> void {
  for (const EmergencyStream &current_stream : emergency_streams) {
    for (const Edge &current_edge : current_stream.route.route) {
      add_et_stream_to_aggregated_et_usage_of_edge(current_edge, current_stream);
    }
  }
}

void NetworkTopology::clear() {
  auto [vi_start, vi_end] = vertices(g);
  std::ranges::for_each(vi_start, vi_end, [&](auto vd) {
    clear_vertex(vd, g);
    remove_vertex(vd, g);
  });
}

void NetworkTopology::print_topology() const {
  std::cout << "Topology" << std::endl;
  for (const auto vd : boost::make_iterator_range(vertices(g))) {
    std::cout << g[vd].id << " --> ";
    auto [out_begin, out_end] = out_edges(vd, g);
    std::ranges::for_each(out_begin, out_end, [&](auto ed) {
      const auto t = target(ed, g);
      std::cout << g[t].id << " ";
    });
    std::cout << std::endl;
  }

  std::cout << "ID\tName\tProcessing Delay\tQueues per Port" << std::endl;
  for (const auto vd : boost::make_iterator_range(vertices(g))) {
    std::cout << g[vd].id << "\t" << g[vd].name << "\t" << g[vd].processing_delay << "\t\t\t" << g[vd].queues_per_port
              << std::endl;
  }
  std::cout << "Edge\tData Rate\tPropagation Delay" << std::endl;
  for (auto ed : make_iterator_range(edges(g))) {
    std::cout << "(" << g[source(ed, g)].id << ", " << g[target(ed, g)].id << ")\t" << g[ed].data_rate << "\t"
              << g[ed].propagation_delay << std::endl;
  }
}

void NetworkTopology::dump_topology(const std::filesystem::path &out) {
  nlohmann::json j = {{"nodes", nlohmann::json::array()}, {"links", nlohmann::json::array()}};

  for (auto vd : boost::make_iterator_range(vertices(g))) {
    j["nodes"].push_back({{"id", g[vd].id}, {"processing_delay_ns", g[vd].processing_delay}, {"name", g[vd].name}});
  }

  for (auto ed : make_iterator_range(edges(g))) {
    j["links"].push_back({{"source", source(ed, g)},
                          {"target", target(ed, g)},
                          {"data_rate", g[ed].data_rate},
                          {"propagation_delay", g[ed].propagation_delay}});
  }

  std::ofstream o(out);
  o << std::setw(4) << j << std::endl;
}

template <bool throw_error>
V NetworkTopology::get_vertex_by_id(const DeviceId id) const {
  for (const auto vd : boost::make_iterator_range(vertices(g))) {
    if (g[vd].id == id)
      return vd;
  }
  if (throw_error)
    throw std::runtime_error{"device not found: id " + std::to_string(id)};
  return network_topology_t::null_vertex();
}

V NetworkTopology::get_vertex_by_id(const std::vector<NetworkDeviceProperty> &device_properties, const DeviceId id) {
  int i = 0;
  for (const NetworkDeviceProperty &device_property : device_properties) {
    if (device_property.id == id)
      return i;
    i++;
  }
  throw std::runtime_error{"device not found: id " + std::to_string(id)};
}

E NetworkTopology::get_edge_by_ids(const Edge &edge) const {
  const V v1 = get_vertex_by_id<true>(edge.first);
  const V v2 = get_vertex_by_id<true>(edge.second);

  if (auto [e, found] = boost::edge(v1, v2, g); found)
    return e;

  throw std::runtime_error{"edge not found: (" + std::to_string(v1) + ", " + std::to_string(v2) + ")"};
}

auto NetworkTopology::get_edge_by_ids(const std::vector<NetworkDeviceProperty> &device_properties,
                                      const Edge &edge) -> std::pair<V, V> {
  V v1 = get_vertex_by_id(device_properties, edge.first);
  V v2 = get_vertex_by_id(device_properties, edge.second);
  return std::make_pair(v1, v2);
}

} // namespace tsndgm
