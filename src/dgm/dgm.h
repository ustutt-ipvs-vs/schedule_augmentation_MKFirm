#pragma once

#include "../network/message_stream.h"
#include "../network/topology.h"
#include "transmission_graph.h"
#include <boost/graph/adjacency_list.hpp>
#include <utility>

namespace tsndgm {

class DisjunctiveGraphModel {
public:
  typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

  transmission_graph_t transmission_graph;
  const NetworkTopology &network;
  std::vector<StreamSchedule> scheduled_streams;
  long hyper_cycle;

  DisjunctiveGraphModel(const NetworkTopology &network, const std::unordered_map<StreamID, MessageStream> &streams,
                        const std::vector<StreamSchedule> &scheduled_streams)
      : network(network), scheduled_streams(scheduled_streams), hyper_cycle(calculateHyperCycle(streams)) {
    transmission_graph[boost::graph_bundle].src = boost::add_vertex(transmission_graph);
    transmission_graph[boost::graph_bundle].sink = boost::add_vertex(transmission_graph);
    transmission_graph[boost::graph_bundle].tt_streams = streams;

    build();
  }

  DisjunctiveGraphModel(const DisjunctiveGraphModel &other) = default;

  auto get_operation_on_edge(const Edge &edge, StreamID stream_id, int frame_number) -> std::optional<V>;

  void print() const { tsndgm::print(transmission_graph, network); }
  void print_operations() const { tsndgm::print_operations(transmission_graph, network); }

  void print(const V v) const { tsndgm::print(transmission_graph, network, v); }
  void print(const E &e) const { tsndgm::print(transmission_graph, network, e); }

  [[nodiscard]] E edge(const V u, const V v) const {
    auto [e, found] = boost::edge(u, v, transmission_graph);
    if (not found)
      throw std::runtime_error("edge (" + std::to_string(u) + ", " + std::to_string(v) + ") does not exist");
    return e;
  }

  [[nodiscard]] E edge(const E &uv) const {
    const V u = source(uv, transmission_graph), v = target(uv, transmission_graph);
    return edge(u, v);
  }

  [[nodiscard]] auto getOutgoingConjunctiveEdge(V v) const -> std::optional<E>;

  [[nodiscard]] auto getOutgoingDisjunctiveEdge(V v) const -> std::optional<E>;

  [[nodiscard]] auto getOutgoingFifoEdge(V v) const -> std::optional<E>;

  [[nodiscard]] auto getIncomingDisjunctiveEdge(V v) const -> std::optional<E>;

  template <TransmissionGraphEdgeType type>
  auto getEdge(auto edges) const -> std::optional<E> {
    for (const auto current_edge : make_iterator_range(edges)) {
      if (transmission_graph[current_edge].edge_type == type) {
        return current_edge;
      }
    }
    return std::nullopt;
  }

  auto checkDeadlineViolations() -> bool;

private:
  auto build() -> void;
  auto add_conjunctive_edges_for_frame(const FrameSchedule &current_frame_schedule, StreamID frame_number, int pcp)
      -> void;
  void add_disjunctive_edges_for_network_link(const Edge &network_link, std::vector<V> vertices);
  void add_fifo_edges_for_network_link(std::vector<V> vertices);
  Delay getTransmissionDelay(const Edge &edge, StreamID stream_id);

  [[nodiscard]] static auto calculateHyperCycle(const std::unordered_map<StreamID, MessageStream> &streams) -> long;
};

} // namespace tsndgm
