#include "outputWriter.h"

#include <fstream>
#include <iostream>
#include <ranges>
#include <src/dgm/dgm.h>
#include <src/dgm/transmission_graph.h>
#include <src/network/message_stream.h>

auto io::write_output(const FilePath &out, const tsndgm::NetworkTopology &network,
                      const tsndgm::DisjunctiveGraphModel &dgm) -> void {

  nlohmann::ordered_json j = nlohmann::ordered_json::object();
  auto networkGraph = network.getNetworkTopology();

  for (const auto vd : boost::make_iterator_range(vertices(networkGraph))) {
    auto device_id = networkGraph[vd].id;
    j[std::to_string(device_id)] = nlohmann::ordered_json::object();
    j[std::to_string(device_id)]["id"] = device_id;
    j[std::to_string(device_id)]["ports"] = createPorts(vd, networkGraph, dgm);
  }

  std::ofstream o(out);
  o << std::setw(4) << j << std::endl;
}

auto io::createPorts(const V v, const tsndgm::network_topology_t &networkGraph,
                     const tsndgm::DisjunctiveGraphModel &dgm) -> nlohmann::ordered_json {
  nlohmann::ordered_json ports = nlohmann::ordered_json::object();

  for (const auto edge : make_iterator_range(out_edges(v, networkGraph))) {
    // TODO Link ID
    auto link_id = std::to_string(edge.m_source) + "-" + std::to_string(edge.m_target);
    ports[link_id] = nlohmann::ordered_json::object();
    ports[link_id]["id"] = link_id;
    ports[link_id]["target"] = edge.m_target;
    ports[link_id]["gcl_per_pcp"] = createGCL(edge, dgm);
  }

  return ports;
}
auto io::createGCL(const E edge, const tsndgm::DisjunctiveGraphModel &dgm) -> nlohmann::ordered_json {
  tsndgm::TransmissionGraphProperty prop = dgm.transmission_graph[boost::graph_bundle];
  const auto e = tsndgm::Edge(edge.m_source, edge.m_target);
  auto vertices = prop.topology_edge_to_dgm_vertices[e];
  auto gcl = nlohmann::ordered_json::object();

  auto grouping_function = [&](const auto lhs_id, const auto rhs_id) {
    const auto &lhs_elem = dgm.transmission_graph[lhs_id];
    const auto &rhs_elem = dgm.transmission_graph[rhs_id];
    return lhs_elem.pcp == rhs_elem.pcp;
  };

  std::ranges::for_each(vertices | std::views::chunk_by(grouping_function), [&](const auto pcp_group) {
    auto array = nlohmann::ordered_json::array();
    for (V v : pcp_group) {
      auto vertexProperty = dgm.transmission_graph[v];
      auto entry_gcl = nlohmann::ordered_json::object();
      auto entry_stream = nlohmann::ordered_json::array();

      entry_stream.push_back(nlohmann::ordered_json::object(
          {{"stream_id", vertexProperty.stream_id}, {"frame_number", vertexProperty.frame_number}}));
      entry_gcl["streams"] = entry_stream;
      entry_gcl["open_time_ns"] = prop.gate_openings[v].first;
      entry_gcl["close_time_ns"] = prop.gate_openings[v].second;
      array.push_back(entry_gcl);
    }
    gcl[std::to_string(dgm.transmission_graph[pcp_group[0]].pcp)] = array;
  });

  return gcl;
}
