#pragma once

#include "../dgm/dgm.h"
#include "../network/emergency_stream.h"
#include "../network/message_stream.h"
#include <filesystem>
#include <vector>

/**
 * collects functions to write the output file (json)
 */
namespace io {

typedef boost::graph_traits<tsndgm::transmission_graph_t>::vertex_descriptor V;
typedef boost::graph_traits<tsndgm::transmission_graph_t>::edge_descriptor E;
typedef std::filesystem::path FilePath;

auto write_output(const FilePath &out, const tsndgm::NetworkTopology &network,
                  const tsndgm::DisjunctiveGraphModel &dgm) -> void;

auto createPorts(V v, const tsndgm::NetworkTopology &network,
                 const tsndgm::DisjunctiveGraphModel &dgm) -> nlohmann::ordered_json;

auto createGCL(const E &edge, const tsndgm::DisjunctiveGraphModel &dgm) -> nlohmann::ordered_json;

} // namespace io
