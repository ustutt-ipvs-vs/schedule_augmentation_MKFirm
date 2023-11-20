#include "dgm.h"
#include "complete_flip.h"
#include <boost/graph/copy.hpp>

namespace tsndgm {

void DisjunctiveGraphModel::complete_flip(std::list<E> edge_list) {
  std::list<E> required_flips = edge_list;
  std::set<OrientationStatePair *> flipped_edges;

  do {
    for (E &e : required_flips) {
      if (flipped_edges.insert(shuffle_graph[e].state_pair->reversed_state)
              .second)
        std::swap(shuffle_graph[e].state_pair->state,
                  shuffle_graph[e].state_pair->reversed_state->state);
    }
    required_flips.clear();

    dgm_traversal(shuffle_graph, visitor(complete_flip_visitor<shuffle_graph_t>(
                                             flipped_edges, required_flips))
                                     .root_vertex(prop.src));
    flipped_edges.clear();
  } while (!required_flips.empty());
}

void DisjunctiveGraphModel::print() { tsndgm::print(shuffle_graph, prop); }

void DisjunctiveGraphModel::build() {
  for (MessageStreamHandle i = 0; i < prop.streams.size(); i++)
    build_stream(i);
  copy_graph();
}

void DisjunctiveGraphModel::build_stream(MessageStreamHandle handle) {
  std::map<MessageStreamHandle, PtrOrientationStatePair>
      prev_orientation_states, cur_orientation_states;

  for (const TreeRouteHop &hop : *prop.streams[handle].route) {
    // add vertex to disjunctive graph
    V v = boost::add_vertex({hop.edge, {handle}, {&hop}}, shuffle_graph);
    prop.operation_to_vertex[{hop.edge, handle}] = v;

    // add conjunctive edge from parent
    if (!hop.parent->is_root()) {
      V v_parent = prop.operation_to_vertex[{hop.parent->edge, handle}];
      boost::add_edge(v_parent, v,
                      {prop.streams[handle].rti_map[hop.parent->edge].d_max(),
                       CONJUNCTIVE_STATE, conjunctive},
                      shuffle_graph);
      shuffle_graph[v].root = shuffle_graph[v_parent].root;
    } else {
      boost::add_edge(
          prop.src, v,
          {prop.streams[handle].phase, CONJUNCTIVE_STATE, conjunctive},
          shuffle_graph);
      shuffle_graph[v].root = {v};
    }

    // add edge to sink
    if (hop.is_leaf()) {
      boost::add_edge(v, prop.sink,
                      {prop.streams[handle].rti_map[hop.edge].d_max(),
                       CONJUNCTIVE_STATE, conjunctive},
                      shuffle_graph);
    }

    // add disjunctive edge for every transmission on the same data link
    auto edge_search = prop.edge_to_streams.find(hop.edge);
    if (edge_search == prop.edge_to_streams.end()) {
      prop.edge_to_streams[hop.edge] = {handle};
    } else {
      // contesting message streams exist
      for (MessageStreamHandle other : edge_search->second) {
        V v_other = prop.operation_to_vertex[{hop.edge, other}];
        PtrOrientationStatePair states;

        auto stream_search = prev_orientation_states.find(other);
        if (stream_search == prev_orientation_states.end()) {
          // open new equivalence class
          states = create_pair();
          cur_orientation_states[other] = states;
        } else {
          // equivalence class exists
          states = stream_search->second;
          cur_orientation_states[other] = states;
        }

        // add disjunctive edge v -> v_other
        boost::add_edge(v, v_other,
                        {prop.streams[handle].rti_map[hop.edge].d_trans_max(),
                         states.first, disjunctive},
                        shuffle_graph);

        // add FIFO edges v -> v_other
        if (!hop.parent->is_root()) {
          V v_parent = prop.operation_to_vertex[{hop.parent->edge, handle}];
          boost::add_edge(
              v_other, v_parent,
              {prop.streams[other].rti_map[hop.edge].d_trans_max() -
                   prop.streams[handle].rti_map[hop.parent->edge].d_min(),
               states.second, fifo},
              shuffle_graph);
        }

        // add disjunctive edge v_other -> v
        boost::add_edge(v_other, v,
                        {prop.streams[other].rti_map[hop.edge].d_trans_max(),
                         states.second, disjunctive},
                        shuffle_graph);

        // add FIFO edges v_other -> v
        if (!shuffle_graph[v_other].hop.front()->parent->is_root()) {
          V v_other_parent = prop.operation_to_vertex[{
              shuffle_graph[v_other].hop.front()->parent->edge, other}];
          boost::add_edge(
              v, v_other_parent,
              {prop.streams[handle].rti_map[hop.edge].d_trans_max() -
                   prop.streams[other]
                       .rti_map
                           [shuffle_graph[v_other].hop.front()->parent->edge]
                       .d_min(),
               states.first, fifo},
              shuffle_graph);
        }
      }

      edge_search->second.insert(handle);
      std::swap(prev_orientation_states, cur_orientation_states);
      cur_orientation_states.clear();
    }
  }
}

void DisjunctiveGraphModel::copy_graph() {
  boost::copy_graph(shuffle_graph, initial_shuffle_graph);

  for (auto ed :
       boost::make_iterator_range(boost::edges(initial_shuffle_graph))) {
    if (initial_shuffle_graph[ed].edge_type == disjunctive &&
        source(ed, initial_shuffle_graph) > target(ed, initial_shuffle_graph)) {
      PtrOrientationStatePair states = create_pair();
      auto ed_rev =
          boost::edge(target(ed, initial_shuffle_graph),
                      source(ed, initial_shuffle_graph), initial_shuffle_graph)
              .first;

      initial_shuffle_graph[ed].state_pair = states.first;
      initial_shuffle_graph[ed_rev].state_pair = states.second;
    }
  }
}

} // namespace tsndgm
