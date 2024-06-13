#pragma once

#include "../IO/inputLoader.h"
#include "../network/message_stream.h"
#include "../network/topology.h"
#include "critical_path.h"
#include "transmission_graph.h"
#include "tsn_configuration.h"
#include <boost/graph/adjacency_list.hpp>
#include <ranges>
#include <utility>

namespace tsndgm {
typedef std::map<Edge, size_t> OffsetMap;

class JitterBoundViolation final : public std::exception {
public:
  JitterBoundViolation(const MessageStreamHandle ms, Edge edge, const Delay bound)
      : ms(ms), edge(std::move(edge)), bound(bound) {}

  const char *what() { return "jitter exceeds the allowed bound"; }

  MessageStreamHandle ms;
  Edge edge;
  Delay bound;
};

class DisjunctiveGraphModel {
public:
  typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

  transmission_graph_t transmission_graph;
  const NetworkTopology &network;
  std::vector<StreamSchedule> scheduled_streams;
  CriticalPath crit_path;

  DisjunctiveGraphModel(const NetworkTopology &network, const std::unordered_map<StreamID, MessageStream> &streams,
                        const std::vector<StreamSchedule> &scheduled_streams)
      : network(network), scheduled_streams(scheduled_streams), crit_path(transmission_graph) {
    transmission_graph[boost::graph_bundle].src = boost::add_vertex(transmission_graph);
    transmission_graph[boost::graph_bundle].sink = boost::add_vertex(transmission_graph);
    transmission_graph[boost::graph_bundle].streams = streams;

    std::map<StreamID, MessageStream> stream_id_map;
    for (const auto &current_stream : streams | std::views::values) {
      stream_id_map[current_stream.id] = current_stream;
    }
    transmission_graph[boost::graph_bundle].stream_id_map = stream_id_map;

    build();
  }

  DisjunctiveGraphModel(const DisjunctiveGraphModel &other)
      : transmission_graph(other.transmission_graph), network(other.network),
        scheduled_streams(other.scheduled_streams), crit_path(transmission_graph) {}

  TSNConfiguration derive_tsn_configuration();

  auto critical_path(CriticalPath::Objective type, bool reverse = true) -> CriticalPath::Result;

  void print() const { tsndgm::print(transmission_graph, network); }

  void print(const V v) const { tsndgm::print(transmission_graph, network, v); }
  void print(const E &e) const { tsndgm::print(transmission_graph, network, e); }

  void print_critical_path(CriticalPath::Objective type) { crit_path.print(critical_path(type), network); }

  void print_fixed_lateness() {
    TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];
    for (MessageStreamHandle ms = 0; ms < prop.streams.size(); ms++) {
      auto &stream = prop.streams[ms];
      const auto listeners = stream.route.get_listeners();

      // compute tardiness of stream's end-to-end latency
      for (Edge listener : listeners) {
        V v_listener = prop.operation_to_vertex[{listener, ms}];
        std::cout << ms << ", (" << listener.first << ", " << listener.second << "): " << stream.phase << " "
                  << stream.deadline << " " << prop.crit_cost[v_listener] << " "
                  << crit_path.get_fixed_lateness(ms, listener) << std::endl;
      }
    }
  }

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

  auto getOutgoingConjunctivEdge(const V v) -> E {
    for (auto current_edge : make_iterator_range(out_edges(v, transmission_graph))) {
      if (transmission_graph[current_edge].edge_type == conjunctive) {
        return current_edge;
      }
    }
  }

  auto getOutgoingDisjunctiveEdge(const V v) -> E {
    for (auto current_edge : make_iterator_range(out_edges(v, transmission_graph))) {
      if (transmission_graph[current_edge].edge_type == disjunctive) {
        return current_edge;
      }
    }
  }

  auto getOutgoingFifoEdge(const V v) -> E {
    for (auto current_edge : make_iterator_range(out_edges(v, transmission_graph))) {
      if (transmission_graph[current_edge].edge_type == fifo) {
        return current_edge;
      }
    }
  }

private:
  bool valid_crit_path = false;

  auto build() -> void;
  auto add_conjunctive_edges_for_frame(const FrameSchedule &current_frame_schedule, StreamID frame_number, int pcp)
      -> void;
  void add_disjunctive_edges_for_edge(const Edge &edge, std::vector<V> vertices);
  void add_fifo_edges_for_edge(const Edge &edge, std::vector<V> vertices);
  Delay getTransmissionDelay(const Edge &edge, StreamID stream_id);

  void resize_properties();

  void internal_commit_all(size_t index);
  void internal_restore_commit(size_t index, bool swap);
  void internal_copy_commit(size_t src_index, size_t dst_index);

  void encode(std::vector<unsigned int> &buf, transmission_graph_t &g, OffsetMap &offset_map);

  void remove_fifo_edges(V u, V v);
  void renew_descriptors();
};
} // namespace tsndgm
