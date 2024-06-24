#pragma once

#include "dgm.h"
#include "transmission_graph.h"
#include "traversal.h"

#include <src/util/constants.h>

namespace tsndgm {

class longest_path_visitor : public boost::default_dfs_visitor {
public:
  virtual ~longest_path_visitor() = default;
  static int total_traversals;
  typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

  explicit longest_path_visitor(DisjunctiveGraphModel &dgm)
      : prop(get_property(dgm.transmission_graph, boost::graph_bundle)), dgm(dgm), network_topology(dgm.network) {
    total_traversals++;
  }

  [[nodiscard]] virtual bool back_edge(E e, const transmission_graph_t &transmission_graph) const {
    throw std::runtime_error("Selection is not complete; disjunctive graph is acyclic.");
  }

  void discover_vertex(V v, const transmission_graph_t &transmission_graph) const {
    prop.crit_cost[v] = 0;
    if (reversed) {
      prop.crit_pred[v] = prop.src;
    } else {
      prop.crit_pred[v] = prop.sink;
    }
  }

  void finish_edge(E uv, const transmission_graph_t &transmission_graph) const {
    V u, v;
    if (reversed) {
      u = source(uv, transmission_graph), v = target(uv, transmission_graph);
    } else {
      v = source(uv, transmission_graph), u = target(uv, transmission_graph);
    }
    const Delay v_cost = prop.crit_cost[v];
    const Delay u_cost = prop.crit_cost[u] + transmission_graph[uv].weight;
    if (u_cost >= v_cost) {
      prop.crit_cost[v] = u_cost;
      prop.crit_pred[v] = u;
    }
  }

  void finish_vertex(V v, const transmission_graph_t &transmission_graph) const {

    if (v == prop.src or v == prop.sink) {
      return;
    }

    // collect references to the required data
    const auto &operation = transmission_graph[v];
    const auto &stream = prop.tt_streams.at(operation.stream_id);
    const auto &network_link = network_topology.get_data_link_property(operation.edge);

    // update gate operations
    const auto gate_opening = prop.crit_cost[v];
    const auto mu_i = getLatestTransmissionStart(transmission_graph, v, network_link, gate_opening);
    const auto current_transmission_Delay = calculateTransmissionDelay(network_link.data_rate, stream.frame_size);
    const Tick gate_closing = mu_i + current_transmission_Delay;
    prop.gate_openings[v] = std::make_pair(gate_opening, gate_closing);

    // update outgoing edges
    for (const auto &out_edge : make_iterator_range(out_edges(v, transmission_graph))) {
      const auto &edge = transmission_graph[out_edge];
      const auto destination = target(out_edge, transmission_graph);
      const auto &destination_property = transmission_graph[destination];
      switch (edge.edge_type) {
      case conjunctive:
        if (destination != prop.sink) {
          // sequential
          auto &next_network_link = network_topology.get_data_link_property(destination_property.edge);
          const auto [b_2, b_2_rate] = getBranchingBurst(network_link, next_network_link);
          const auto burst_part = static_cast<Delay>((b_2 + (mu_i - gate_opening) * b_2_rate) / network_link.data_rate);
          // this access is necessary, since the transmission graph given in finish_vertex is const
          dgm.transmission_graph[out_edge].weight = burst_part + getTotalDelay(v, transmission_graph);
        }
        break;
      case disjunctive:
        if (destination_property.pcp > operation.pcp) {
          // deferred
          // add one IFG so that the next TT frame is not transmitted back-to-back
          dgm.transmission_graph[out_edge].weight =
              std::max(mu_i + EPSILON - gate_opening, current_transmission_Delay) + network_link.getInterFrameGap();
        }
        break;
      case fifo:
        // frame isolation
        dgm.transmission_graph[out_edge].weight =
            gate_closing - gate_opening - getTotalDelay(destination, transmission_graph);
        break;
      }
    }
  }

  /**
   * computes mu_i_1 and mu_i_2, returns the max (mu_i)
   * @return
   */
  [[nodiscard]] auto getLatestTransmissionStart(const transmission_graph_t &transmission_graph, const V v,
                                                const DataLinkProperty &network_link, const Tick gate_opening) const
      -> Tick {
    const auto mu_i_1 = gate_opening + calculateTransmissionDelay(network_link.data_rate -
                                                                      network_link.aggregated_emergency_refill_rate,
                                                                  network_link.aggregated_emergency_burst_size);
    const auto in_edge_opt = dgm.getIncommingDisjunctiveEdge(v);
    const Tick mu_i_2 = [&] -> Tick {
      if (in_edge_opt.has_value()) {
        const auto predecessor = in_edge_opt.value().m_source;
        const auto predecessor_closing = prop.gate_openings[predecessor].second;
        const auto &predecessor_stream = prop.tt_streams.at(transmission_graph[predecessor].stream_id);
        return static_cast<Tick>(predecessor_closing +
                                 (calculateTransmissionDelay(network_link.data_rate, predecessor_stream.frame_size) *
                                  network_link.aggregated_emergency_refill_rate) /
                                     (network_link.data_rate - network_link.aggregated_emergency_refill_rate));
      }
      // no disjunctive predecessor
      return 0UL;
    }();

    return std::max(mu_i_1, mu_i_2);
  }

  [[nodiscard]] static auto getBranchingBurst(const DataLinkProperty &pre_branch_link,
                                              const DataLinkProperty &post_branch_link)
      -> std::pair<BurstSize, DataRate> {
    auto view = pre_branch_link.emergency_streams | std::views::filter([&](const auto &stream) {
                  // keep only the ones not present in the post branch link
                  return std::ranges::find_if(post_branch_link.emergency_streams, [&](const auto &post_stream) {
                           return post_stream.get().id == stream.get().id;
                         }) == post_branch_link.emergency_streams.end();
                });
    return std::transform_reduce(
        view.begin(), view.end(), std::pair<BurstSize, DataRate>{0, 0},
        [&](const auto &acc, const auto stream) {
          return std::make_pair(acc.first + stream.first, acc.second + stream.second);
        },
        [&](const auto wrapped_stream) {
          return std::make_pair(wrapped_stream.get().bucket_size_byte, wrapped_stream.get().refill_rate);
        });
  }

  [[nodiscard]] auto getTotalDelay(const V operation, const transmission_graph_t &transmission_graph) const -> Delay {
    const auto &operation_property = transmission_graph[operation];
    const auto &network_link = network_topology.get_data_link_property(operation_property.edge);
    const auto propagation_delay = network_topology.get_device_property(operation_property.edge.first).processing_delay;
    const auto transmission_delay =
        calculateTransmissionDelay(network_link.data_rate, prop.tt_streams.at(operation_property.stream_id).frame_size);
    const auto processing_delay = network_link.propagation_delay;
    return propagation_delay + transmission_delay + processing_delay;
  }

  TransmissionGraphProperty &prop;
  DisjunctiveGraphModel &dgm;
  const NetworkTopology &network_topology;
  bool reversed = true;
};

class slack_visitor final : public boost::default_dfs_visitor {
public:
  virtual ~slack_visitor() = default;
  typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

  explicit slack_visitor(transmission_graph_t &transmission_graph)
      : prop(get_property(transmission_graph, boost::graph_bundle)) {
    longest_path_visitor::total_traversals++;
  }

  [[nodiscard]] virtual bool back_edge(E e, const transmission_graph_t &transmission_graph) const {
    throw std::runtime_error("Selection is not complete; disjunctive graph is acyclic.");
  }

  void discover_vertex(V v, const transmission_graph_t &transmission_graph) const {
    if (v == prop.sink || boost::edge(v, prop.sink, transmission_graph).second)
      prop.slack[v] = 0;
    else
      prop.slack[v] = std::numeric_limits<Delay>::max();
  }

  void finish_edge(E uv, const transmission_graph_t &transmission_graph) const {
    V u = source(uv, transmission_graph), v = target(uv, transmission_graph);

    if (transmission_graph[uv].weight == std::numeric_limits<Delay>::min())
      return;

    Delay uv_slack = prop.crit_cost[v] - prop.crit_cost[u] - transmission_graph[uv].weight;
    if (uv_slack + prop.slack[v] < prop.slack[u])
      prop.slack[u] = uv_slack + prop.slack[v];
  }

  TransmissionGraphProperty &prop;
  bool reversed = false;
};

} // namespace tsndgm
