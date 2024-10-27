#include "dgm.h"
#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/copy.hpp>
#include <numeric>
#include <ranges>

namespace tsndgm {

auto DisjunctiveGraphModel::get_operation_on_edge(const Edge &edge, const StreamID stream_id, const int frame_number)
    -> std::optional<V> {
  const TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];

  if (const auto iter = std::ranges::find_if(prop.topology_edge_to_dgm_vertices.at(edge),
                                             [&](const auto vertex) {
                                               return transmission_graph[vertex].stream_id == stream_id &&
                                                      transmission_graph[vertex].frame_number == frame_number;
                                             });
      iter != prop.topology_edge_to_dgm_vertices.at(edge).end()) {
    return *iter;
  }
  return std::nullopt;
}

auto DisjunctiveGraphModel::getOutgoingConjunctiveEdge(const V v) const -> std::optional<E> {
  return getEdge<conjunctive>(out_edges(v, transmission_graph));
}

auto DisjunctiveGraphModel::getOutgoingDisjunctiveEdge(const V v) const -> std::optional<E> {
  return getEdge<disjunctive>(out_edges(v, transmission_graph));
}
auto DisjunctiveGraphModel::getOutgoingFifoEdge(const V v) const -> std::optional<E> {
  return getEdge<fifo>(out_edges(v, transmission_graph));
}
auto DisjunctiveGraphModel::getIncommingDisjunctiveEdge(const V v) const -> std::optional<E> {
  return getEdge<disjunctive>(in_edges(v, transmission_graph));
}

auto DisjunctiveGraphModel::computeGateOpeningAndCloseOperations() -> void {
  // TODO probably we can remove this function. Now the gate oerations are computed in the graph_visitor.
  TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];

  for (const auto &[network_link, operations] : prop.topology_edge_to_dgm_vertices) {

    for (const auto &operation : operations) {
      const auto stream_id = transmission_graph[operation].stream_id;
      const auto dtrans = getTransmissionDelay(network_link, stream_id);
      const auto vertex_prop = transmission_graph[operation];
      // TODO should we adhere to the old schedule?
      const auto open_time = std::max(vertex_prop.start_old_schedule, prop.crit_cost.at(operation));
      const auto close_time = open_time + dtrans;
      prop.gate_openings[operation] = std::make_pair(open_time, close_time);
    }
  }
}
auto DisjunctiveGraphModel::checkDeadlineViolations() -> bool {
  const TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];

  const auto final_edges = make_iterator_range(in_edges(prop.sink, transmission_graph));
  bool deadline_violation = false;

  for (const auto edge : final_edges) {
    const auto source_v = source(edge, transmission_graph);
    const auto &operation = transmission_graph[source_v];
    const auto &stream = prop.tt_streams.at(operation.stream_id);
    const auto &network_link = network.get_data_link_property(operation.edge);
    const auto actual_arrival = prop.gate_openings[source_v].second + network_link.propagation_delay;
    const auto deadline = transmission_graph[source_v].frame_number * stream.period + stream.deadline;
    if (deadline < actual_arrival) {
      std::cout << "stream: " << operation.stream_id << " frame " << operation.frame_number
                << " violates it's deadline: " << actual_arrival << " instead of " << stream.deadline << "\n";
      deadline_violation = true;
    }
  }
  return deadline_violation;
}

auto DisjunctiveGraphModel::build() -> void {
  TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];

  // add conjunctive Edges
  for (const StreamSchedule &current_stream : scheduled_streams) {
    for (const FrameSchedule &current_frame_schedule : current_stream.frames) {
      add_conjunctive_edges_for_frame(current_frame_schedule, current_stream.stream_id, current_stream.pcp);
    }
  }

  // add disjunctive and FIFO Edges
  for (const auto &[network_link, vertex_list] : prop.topology_edge_to_dgm_vertices) {
    add_disjunctive_edges_for_network_link(network_link, vertex_list);
    add_fifo_edges_for_network_link(vertex_list);
  }

  prop.resize(num_vertices(transmission_graph));
}

auto DisjunctiveGraphModel::add_conjunctive_edges_for_frame(const FrameSchedule &current_frame_schedule,
                                                            const StreamID frame_number, const int pcp) -> void {
  TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];
  V previous_vertex = prop.src;

  // Weight of first conjunctive edge = dRelease (start time / gate open)
  unsigned int weight_previous_iteration = current_frame_schedule.transmissions.front().start;
  for (const FrameTransmission &current_transmission : current_frame_schedule.transmissions) {
    // add vertex to disjunctive graph
    auto transmission_edge = Edge(current_transmission.source, current_transmission.target);
    const V new_vertex = boost::add_vertex({transmission_edge, previous_vertex, frame_number,
                                            current_frame_schedule.frame_number, current_transmission.start, pcp},
                                           transmission_graph);

    prop.topology_edge_to_dgm_vertices[transmission_edge].push_back(new_vertex);

    // Add edge, use weight calculated in previous iteration
    boost::add_edge(previous_vertex, new_vertex, {weight_previous_iteration, conjunctive}, transmission_graph);

    const auto dprop = network.get_data_link_property(transmission_edge).propagation_delay;
    const auto dtrans = getTransmissionDelay(transmission_edge, frame_number);
    // Processing delay of next bridge
    const auto dproc = network.get_device_property(current_transmission.target).processing_delay;

    weight_previous_iteration = dprop + dtrans + dproc;
    previous_vertex = new_vertex;
  }

  // add edge to sink using last weight calculated in loop
  boost::add_edge(previous_vertex, prop.sink, {weight_previous_iteration, conjunctive}, transmission_graph);
}

void DisjunctiveGraphModel::add_disjunctive_edges_for_network_link(const Edge &network_link, std::vector<V> vertices) {
  // here we create a function (lambda) stored in "sorting_function", which we
  // will pass to the actual sorting algorithm.
  auto ordering_function = [&](const auto lhs_id, const auto rhs_id) {
    const auto &lhs_elem = transmission_graph[lhs_id];
    const auto &rhs_elem = transmission_graph[rhs_id];

    return lhs_elem.start_old_schedule < rhs_elem.start_old_schedule;
  };

  std::ranges::sort(vertices, ordering_function);

  for (const auto &[first_vertex, second_vertex] : vertices | std::views::pairwise) {
    const auto ifg = network.get_data_link_property(transmission_graph[first_vertex].edge).getInterFrameGap();
    const auto dtrans = getTransmissionDelay(network_link, transmission_graph[first_vertex].stream_id);
    boost::add_edge(first_vertex, second_vertex, {dtrans + ifg, disjunctive}, transmission_graph);
  }
}

auto DisjunctiveGraphModel::add_fifo_edges_for_network_link(std::vector<V> vertices) -> void {
  TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];

  auto ordering_function = [&](const auto lhs_id, const auto rhs_id) {
    // sort by pcp first and old start time second.
    const auto &lhs_elem = transmission_graph[lhs_id];
    const auto &rhs_elem = transmission_graph[rhs_id];

    if (lhs_elem.pcp == rhs_elem.pcp) {
      // sort by start time as secondary ordering
      return lhs_elem.start_old_schedule < rhs_elem.start_old_schedule;
    }
    return lhs_elem.pcp < rhs_elem.pcp;
  };

  auto grouping_function = [&](const auto lhs_id, const auto rhs_id) {
    const auto &lhs_elem = transmission_graph[lhs_id];
    const auto &rhs_elem = transmission_graph[rhs_id];
    return lhs_elem.pcp == rhs_elem.pcp;
  };

  std::ranges::sort(vertices, ordering_function);

  std::ranges::for_each(vertices | std::views::chunk_by(grouping_function), [&](const auto pcp_group) {
    for (auto filtered =
             pcp_group | std::views::pairwise | std::views::filter([&](const auto &pair) {
               // filter src -> transmission edges
               return transmission_graph[std::get<0>(pair)].dgm_predecessor_vertex != prop.src;
             }) |
             std::views::filter([&](const auto &pair) {
	       auto [v1, v2] = pair;
               // filter frames where the early frame is delivered before the later frame is released
               const MessageStream &early_stream = prop.tt_streams.at(transmission_graph[v1].stream_id);
               const MessageStream &late_stream = prop.tt_streams.at(transmission_graph[v2].stream_id);
               const auto deadline_early_stream =
                   early_stream.period * transmission_graph[v1].frame_number + early_stream.deadline;
               const auto release_time_late_stream = late_stream.period * transmission_graph[v2].frame_number;
               return release_time_late_stream < deadline_early_stream;
             });
         const auto [first_vertex, second_vertex] : filtered) {
      // add FIFO edge from vertex with lower start time to predecessor of vertex with higher start time
      const auto dtrans =
          getTransmissionDelay(transmission_graph[first_vertex].edge, transmission_graph[first_vertex].stream_id);
      const auto transmission_edge =
          boost::edge(transmission_graph[second_vertex].dgm_predecessor_vertex, second_vertex, transmission_graph)
              .first;
      const auto weight = dtrans - transmission_graph[transmission_edge].weight;

      boost::add_edge(first_vertex, transmission_graph[second_vertex].dgm_predecessor_vertex, {weight, fifo},
                      transmission_graph);
    }
  });
}

auto DisjunctiveGraphModel::getTransmissionDelay(const Edge &edge, const StreamID stream_id) -> Delay {
  const TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];

  const DataRate data_rate = network.get_data_link_property(edge).data_rate;
  const FrameSize frame_size = prop.tt_streams.at(stream_id).frame_size;

  return calculateTransmissionDelay(data_rate, frame_size);
}

auto DisjunctiveGraphModel::calculateHyperCycle(const std::unordered_map<StreamID, MessageStream> &streams) -> long {

  const auto stream_view =
      streams | std::views::values | std::views::transform([](const auto &stream) { return stream.period; });
  return std::reduce(std::cbegin(stream_view), std::cend(stream_view), 1l,
                     [](const auto lhs, const auto rhs) { return std::lcm(lhs, rhs); });
}

} // namespace tsndgm
