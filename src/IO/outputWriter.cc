#include "outputWriter.h"

#include <fstream>
#include <ranges>
#include <src/dgm/dgm.h>
#include <src/dgm/transmission_graph.h>
#include <src/network/message_stream.h>

auto io::write_output(const FilePath &out, const tsndgm::NetworkTopology &network,
                      const tsndgm::DisjunctiveGraphModel &dgm) -> void {

  nlohmann::ordered_json j = nlohmann::ordered_json::object();
  const auto &networkGraph = network.getNetworkTopology();

  for (const auto vd : boost::make_iterator_range(vertices(networkGraph))) {
    auto device_id = networkGraph[vd].id;
    j[std::to_string(device_id)] = nlohmann::ordered_json::object();
    j[std::to_string(device_id)]["id"] = device_id;
    j[std::to_string(device_id)]["name"] = networkGraph[vd].name;
    j[std::to_string(device_id)]["ports"] = createPorts(vd, network, dgm);
  }

  std::ofstream o(out);
  o << std::setw(4) << j << std::endl;
}

auto io::createPorts(const V v, const tsndgm::NetworkTopology &network, const tsndgm::DisjunctiveGraphModel &dgm)
    -> nlohmann::ordered_json {
  nlohmann::ordered_json ports = nlohmann::ordered_json::object();
  const auto &networkGraph = network.getNetworkTopology();

  for (const auto edge : make_iterator_range(out_edges(v, networkGraph))) {
    const auto edgeProperty = network.get_data_link_property(tsndgm::Edge(edge.m_source, edge.m_target));
    const auto link_key = std::to_string(edgeProperty.id);
    ports[link_key] = nlohmann::ordered_json::object();
    ports[link_key]["id"] = edgeProperty.id;
    ports[link_key]["name"] = edgeProperty.name;
    ports[link_key]["target"] = edge.m_target;
    ports[link_key]["gcl_per_pcp"] = createGCL(edge, dgm);
  }

  return ports;
}
auto io::createGCL(const E &edge, const tsndgm::DisjunctiveGraphModel &dgm) -> nlohmann::ordered_json {
  const auto &prop = dgm.transmission_graph[boost::graph_bundle];
  const auto e = tsndgm::Edge(edge.m_source, edge.m_target);
  auto gcl = nlohmann::ordered_json::object();

  auto grouping_function = [&](const auto lhs_id, const auto rhs_id) {
    const auto &lhs_elem = dgm.transmission_graph[lhs_id];
    const auto &rhs_elem = dgm.transmission_graph[rhs_id];
    return lhs_elem.pcp == rhs_elem.pcp;
  };

  if (prop.topology_edge_to_dgm_vertices.contains(e)) {
    const auto &vertices = prop.topology_edge_to_dgm_vertices.at(e);
    std::ranges::for_each(vertices | std::views::chunk_by(grouping_function), [&](const auto pcp_group) {
      auto array = nlohmann::ordered_json::array();
      for (const V v : pcp_group) {
        const auto &vertexProperty = dgm.transmission_graph[v];
        auto entry_gcl = nlohmann::ordered_json::object();
        auto entry_stream = nlohmann::ordered_json::array();

        entry_stream.push_back(nlohmann::ordered_json::object(
            {{"stream_id", vertexProperty.stream_id}, {"frame_number", vertexProperty.frame_number}}));
        entry_gcl["streams"] = entry_stream;
        entry_gcl["open_time_ns"] = prop.gate_openings.at(v).first;
        entry_gcl["close_time_ns"] = prop.gate_openings.at(v).second;
        array.push_back(entry_gcl);
      }
      gcl[std::to_string(dgm.transmission_graph[pcp_group.front()].pcp)] = array;
    });
  }

  return gcl;
}
