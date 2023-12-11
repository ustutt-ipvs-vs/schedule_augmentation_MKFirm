#include "dgm.h"
#include "complete_flip.h"
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/copy.hpp>

namespace tsndgm {

CriticalPath::Result
DisjunctiveGraphModel::critical_path(CriticalPath::Objective type) {
  if (!valid_crit_path)
    crit_path.compute_longest_paths();
  return crit_path.path(type);
}

void DisjunctiveGraphModel::print_critical_path(CriticalPath::Objective type) {
  crit_path.print(critical_path(type));
}

void DisjunctiveGraphModel::commit_flips() { flip_log.clear(); }

void DisjunctiveGraphModel::commit_all(size_t index) {
  if (index == 0)
    flip_log.clear();

  for (size_t i = committed_shuffle_graphs.size(); i <= index; i++)
    committed_shuffle_graphs.push_back(shuffle_graph_t());

  committed_shuffle_graphs[index].clear();
  boost::copy_graph(shuffle_graph, committed_shuffle_graphs[index]);
  committed_shuffle_graphs[index][boost::graph_bundle] =
      shuffle_graph[boost::graph_bundle];

  for (E e : boost::make_iterator_range(boost::edges(shuffle_graph))) {
    if (shuffle_graph[e].edge_type == disjunctive) {
      shuffle_graph[e].state_pair->commit(index);
    }
  }
}

void DisjunctiveGraphModel::restore_flips() {
  for (E e : flip_log)
    shuffle_graph[e].consistent_flip();
  flip_log.clear();
  valid_crit_path = false;
}

void DisjunctiveGraphModel::restore_commit(size_t index, bool swap) {
  flip_log.clear();
  if (boost::num_vertices(committed_shuffle_graphs[index]) > 0) {
    shuffle_graph.clear();
    if (swap) {
      std::swap(committed_shuffle_graphs[index], shuffle_graph);
    } else {
      boost::copy_graph(committed_shuffle_graphs[index], shuffle_graph);
      shuffle_graph[boost::graph_bundle] =
          committed_shuffle_graphs[index][boost::graph_bundle];
    }

    for (E e : boost::make_iterator_range(boost::edges(shuffle_graph))) {
      if (shuffle_graph[e].edge_type == disjunctive) {
        shuffle_graph[e].state_pair->restore_commit(index, swap);
      }
    }
  }
  valid_crit_path = false;
}

void DisjunctiveGraphModel::copy_commit(size_t src_index, size_t dest_index) {
  boost::copy_graph(committed_shuffle_graphs[src_index],
                    committed_shuffle_graphs[dest_index]);
  boost::set_property(committed_shuffle_graphs[dest_index], boost::graph_bundle,
                      boost::get_property(committed_shuffle_graphs[src_index],
                                          boost::graph_bundle));

  for (E e : boost::make_iterator_range(
           boost::edges(committed_shuffle_graphs[dest_index]))) {
    if (committed_shuffle_graphs[dest_index][e].edge_type == disjunctive) {
      committed_shuffle_graphs[dest_index][e].state_pair->copy_commit(
          src_index, dest_index);
    }
  }
}

void DisjunctiveGraphModel::swap_commit(size_t src_index, size_t dest_index) {
  std::swap(committed_shuffle_graphs[src_index],
            committed_shuffle_graphs[dest_index]);
  for (E e : boost::make_iterator_range(
           boost::edges(committed_shuffle_graphs[dest_index]))) {
    if (committed_shuffle_graphs[dest_index][e].edge_type == disjunctive) {
      committed_shuffle_graphs[dest_index][e].state_pair->swap_commit(
          src_index, dest_index);
    }
  }
}

void DisjunctiveGraphModel::complete_flip(
    const std::list<E> &edge_list,
    std::set<boost::OrientationState *> &flipped_edges) {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  std::list<E> required_flips = edge_list;
  std::set<boost::OrientationState *> required_flips_classes;

  do {
    for (E &e : required_flips) {
      flipped_edges.insert(shuffle_graph[e].state_pair->reversed_state.get());
      shuffle_graph[e].consistent_flip();
      flip_log.push_back(e);
    }
    required_flips.clear();
    required_flips_classes.clear();

    reversed_dgm_traversal(
        shuffle_graph,
        visitor(complete_flip_visitor(shuffle_graph, flipped_edges,
                                      required_flips_classes, required_flips))
            .root_vertex(prop.sink));
  } while (!required_flips.empty());

  valid_crit_path = true;
}

void DisjunctiveGraphModel::lazy_shuffle(E edge) {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];

  // invalidate flip_log
  flip_log.clear();

  // gather equivalence class of edge
  std::list<E> related_edges;
  for (E e : boost::make_iterator_range(boost::edges(shuffle_graph))) {
    if (shuffle_graph[e].edge_type == disjunctive &&
        shuffle_graph[edge].relates_to(shuffle_graph[e])) {
      related_edges.push_back(e);
    }
  }

  std::set<boost::OrientationState *> flipped_edges;

  // By shuffling uv, we relay all edges from and to v over u.
  // To avoid invalidating vertex and edge descriptors, however, we
  // do not remove v from the graph.
  for (E uv : related_edges) {
    V u = source(uv, shuffle_graph), v = target(uv, shuffle_graph);
    Edge edge = shuffle_graph[u].edge;

    // remove fifo edges
    remove_fifo_edges(u, v);
    remove_fifo_edges(v, u);

    // adjust operation_to_vertex mapping
    for (MessageStreamHandle ms_handle : shuffle_graph[v].ms_handle)
      prop.operation_to_vertex[Operation(shuffle_graph[v].edge, ms_handle)] = u;
    // merge properties of u and v
    shuffle_graph[u].shuffle(shuffle_graph[v]);

    // Edge contraction and weight adjustment
    // At the same time, we update the edge equivalence classes
    for (E vw :
         boost::make_iterator_range(boost::out_edges(v, shuffle_graph))) {
      V w = target(vw, shuffle_graph);
      if (w == u)
        continue;
      if (shuffle_graph[vw].edge_type == conjunctive) {
        // edge v -> w becomes u -> w
        E uw = boost::add_edge(u, w, shuffle_graph[vw], shuffle_graph).first;
        shuffle_graph[uw].weight += shuffle_graph[vw].weight;
      } else if (shuffle_graph[vw].edge_type == disjunctive) {
        // Edge u -> w must already exist.
        // Further the equivalence classes of u -> w and v -> w must be merged.
        // Hence, if u -> w and v -> w have different orientations, we flip the
        // old equivalence class of u -> w
        E uw = boost::edge(u, w, shuffle_graph).first;
        if (shuffle_graph[uw].state() != shuffle_graph[vw].state())
          flipped_edges.insert(shuffle_graph[uw].state_pair->state.get());
        shuffle_graph[vw].merge_equivalence_classes(uw, shuffle_graph, prop);
        shuffle_graph[uw].weight += shuffle_graph[vw].weight;
      } else {
        // Edge u -> w does not necessarily exist (e.g., if parent(u).edge !=
        // parent(v).edge). The, we need to create a new fifo edge and add it
        // to the equivalence class vw. Note that merging the correct
        // equivalence classes is done in iterations where e vw is a disjunctive
        // edge.
        auto uw = boost::edge(u, w, shuffle_graph);
        if (!uw.second) {
          uw = boost::add_edge(u, w, shuffle_graph[vw], shuffle_graph);
          shuffle_graph[uw.first].state_pair->add_edge(uw.first, shuffle_graph);
        }
        shuffle_graph[uw.first].weight += shuffle_graph[uv].weight;
      }
    }
    // Symmetric operations for ingoing edges.
    // However, except for the fifo case, we do not need to modify the weights
    // here.
    for (E wv : boost::make_iterator_range(boost::in_edges(v, shuffle_graph))) {
      V w = source(wv, shuffle_graph);
      if (w == u)
        continue;
      if (shuffle_graph[wv].edge_type == conjunctive) {
        if (!boost::edge(w, u, shuffle_graph).second)
          boost::add_edge(w, u, shuffle_graph[wv], shuffle_graph);
      } else if (shuffle_graph[wv].edge_type == disjunctive) {
        E wu = boost::edge(w, u, shuffle_graph).first;
        if (shuffle_graph[wu].state() != shuffle_graph[wv].state())
          flipped_edges.insert(shuffle_graph[wu].state_pair->state.get());
        shuffle_graph[wv].merge_equivalence_classes(wu, shuffle_graph, prop);
      } else {
        auto wu = boost::edge(w, u, shuffle_graph);
        if (!wu.second) {
          wu = boost::add_edge(w, u, shuffle_graph[wv], shuffle_graph);
          shuffle_graph[wu.first].state_pair->add_edge(wu.first, shuffle_graph);
        } else {
          shuffle_graph[wu.first].weight = std::min(
              shuffle_graph[wu.first].weight, shuffle_graph[wv].weight);
        }
      }
    }
  }
  // remove edges to speed up DFS, but do not remove vertex to avoid
  // invalidating vertex and edge descriptors
  for (E uv : related_edges) {
    boost::clear_vertex(target(uv, shuffle_graph), shuffle_graph);
  }

  // eliminate cycles that were introduced by merging equivalence classes
  complete_flip({}, flipped_edges);
}

void DisjunctiveGraphModel::split_all() {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];

  // Adjust orientations
  for (E uv :
       boost::make_iterator_range(boost::edges(committed_shuffle_graphs[0]))) {
    if (committed_shuffle_graphs[0][uv].state() != boost::allowed ||
        committed_shuffle_graphs[0][uv].edge_type != disjunctive)
      continue;

    V ui = source(uv, committed_shuffle_graphs[0]),
      vi = target(uv, committed_shuffle_graphs[0]);
    V u = prop.operation_to_vertex[Operation(
          committed_shuffle_graphs[0][ui].edge,
          committed_shuffle_graphs[0][ui].ms_handle.front())],
      v = prop.operation_to_vertex[Operation(
          committed_shuffle_graphs[0][vi].edge,
          committed_shuffle_graphs[0][vi].ms_handle.front())];

    if (u != v &&
        committed_shuffle_graphs[0][uv].state() !=
            shuffle_graph[boost::edge(u, v, shuffle_graph).first].state())
      committed_shuffle_graphs[0][uv].consistent_flip();
    else if (u == v && ui < vi)
      committed_shuffle_graphs[0][uv].consistent_flip();
  }

  // Copy initial_shuffle_graph to shuffle_graph and reset equivalence classes.
  restore_commit(0, false);

  valid_crit_path = false;
}

void DisjunctiveGraphModel::remove_fifo_edges(V u, V v) {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];

  std::list<MessageStreamHandle>::const_iterator ms_it;
  std::list<const TreeRouteHop *>::iterator hop_it;
  for (ms_it = shuffle_graph[v].ms_handle.begin(),
      hop_it = shuffle_graph[v].hop.begin();
       ms_it != shuffle_graph[v].ms_handle.end() &&
       hop_it != shuffle_graph[v].hop.end();
       ++ms_it, ++hop_it) {
    if ((*hop_it)->parent->is_root())
      continue;

    auto search = boost::edge(
        u, prop.operation_to_vertex[Operation((*hop_it)->parent->edge, *ms_it)],
        shuffle_graph);
    if (search.second) {
      boost::remove_edge(search.first, shuffle_graph);
    }
  }
}

void DisjunctiveGraphModel::print() {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  tsndgm::print(shuffle_graph, prop);
}

void DisjunctiveGraphModel::build() {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  for (MessageStreamHandle i = 0; i < prop.streams.size(); i++)
    build_stream(i);
  resize_properties();

  commit_all(0);
}

void DisjunctiveGraphModel::resize_properties() {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  prop.crit_cost.resize(boost::num_vertices(shuffle_graph));
  prop.crit_pred.resize(boost::num_vertices(shuffle_graph));
  prop.cycle_pred.resize(boost::num_vertices(shuffle_graph));
}

void DisjunctiveGraphModel::build_stream(MessageStreamHandle handle) {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
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
        V u = prop.operation_to_vertex[{hop.edge, other}];
        TreeRouteHop *v_hop_parent = hop.parent,
                     *u_hop_parent = shuffle_graph[u].hop.front()->parent;

        std::pair<std::shared_ptr<PtrOrientationStatePair>,
                  std::shared_ptr<PtrOrientationStatePair>>
            states;

        if (!v_hop_parent->is_root() && !u_hop_parent->is_root() &&
            v_hop_parent->edge == u_hop_parent->edge) {
          V v_parent = prop.operation_to_vertex[{v_hop_parent->edge, handle}],
            u_parent = prop.operation_to_vertex[{u_hop_parent->edge, other}];
          E e_parent = boost::edge(v_parent, u_parent, shuffle_graph).first,
            e_rev_parent = boost::edge(u_parent, v_parent, shuffle_graph).first;

          states = std::make_pair(shuffle_graph[e_parent].state_pair,
                                  shuffle_graph[e_rev_parent].state_pair);
        } else {
          // open new equivalence class
          std::shared_ptr<PtrOrientationStatePair> state =
              std::make_shared<PtrOrientationStatePair>(create_pair());
          std::shared_ptr<PtrOrientationStatePair> reversed_state =
              std::make_shared<PtrOrientationStatePair>(
                  PtrOrientationStatePair(state->reversed_state, state->state));
          states = std::make_pair(state, reversed_state);
        }

        // add FIFO edges v -> u
        if (!hop.parent->is_root()) {
          V v_parent = prop.operation_to_vertex[{v_hop_parent->edge, handle}];
          E ed =
              boost::add_edge(
                  u, v_parent,
                  {prop.streams[other].rti_map[hop.edge].d_trans_max() -
                       prop.streams[handle].rti_map[v_hop_parent->edge].d_min(),
                   states.second, fifo},
                  shuffle_graph)
                  .first;
          shuffle_graph[ed].state_pair->add_edge(ed, shuffle_graph);
        }

        // add FIFO edges u -> v
        if (!u_hop_parent->is_root()) {
          V u_parent = prop.operation_to_vertex[{u_hop_parent->edge, other}];
          E ed =
              boost::add_edge(
                  v, u_parent,
                  {prop.streams[handle].rti_map[hop.edge].d_trans_max() -
                       prop.streams[other].rti_map[u_hop_parent->edge].d_min(),
                   states.first, fifo},
                  shuffle_graph)
                  .first;
          shuffle_graph[ed].state_pair->add_edge(ed, shuffle_graph);
        }

        // add disjunctive edge v -> u
        E ed = boost::add_edge(
                   v, u,
                   {prop.streams[handle].rti_map[hop.edge].d_trans_max(),
                    states.first, disjunctive},
                   shuffle_graph)
                   .first;
        shuffle_graph[ed].state_pair->add_edge(ed, shuffle_graph);

        // add disjunctive edge u -> v
        ed = boost::add_edge(
                 u, v,
                 {prop.streams[other].rti_map[hop.edge].d_trans_max(),
                  states.second, disjunctive},
                 shuffle_graph)
                 .first;
        shuffle_graph[ed].state_pair->add_edge(ed, shuffle_graph);
      }
      edge_search->second.insert(handle);
    }
  }
}

} // namespace tsndgm
