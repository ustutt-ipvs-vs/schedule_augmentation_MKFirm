#include "dgm.h"
#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/copy.hpp>
#include <numeric>

namespace tsndgm {

TSNConfiguration DisjunctiveGraphModel::derive_tsn_configuration() {
  return TSNConfiguration(shuffle_graph, *network);
}

CriticalPath::Result
DisjunctiveGraphModel::critical_path(CriticalPath::Objective type,
                                     bool reverse) {
  if (!reverse)
    crit_path.compute_longest_paths(reverse);
  else if (!valid_crit_path)
    update_machine_successors();

  return crit_path.path(type);
}

void DisjunctiveGraphModel::update_rti(
    std::map<MessageStreamHandle, RTIMap> rti_updates) {
  if (rti_updates.empty())
    return;

  internal_restore_commit(initial, false);

  for (auto &[ms, rti_map] : rti_updates)
    update_rti(ms, rti_map);

  committed_shuffle_graphs.clear();
  internal_commit_all(initial);
}

void DisjunctiveGraphModel::update_rti(MessageStreamHandle ms, RTIMap rti_map) {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];

  // ensure that wireline delays remain intact (operation is idempotent)
  for (auto &[edge, rti] : rti_map) {
    rti.add_wireline(prop.streams[ms].rti_map[edge].d_wireline());
  }

  // F2 requires that we also update the weight of JP[v] -> v. To this end, we
  // extend rti_map to contain all relevant job predecessors
  for (auto &[edge, rti] : rti_map) {
    V v = prop.operation_to_vertex[{edge, ms}];
    for (auto &nv : shuffle_graph[v].JP) {
      V u = nv.v;
      Edge edge = shuffle_graph[u].edge;
      bool u_contains_ms = std::find(shuffle_graph[u].ms_handle.begin(),
                                     shuffle_graph[u].ms_handle.end(),
                                     ms) != shuffle_graph[u].ms_handle.end();
      if (u_contains_ms && !rti_map.contains(edge)) {
        rti_map[edge] = prop.streams[ms].rti_map[edge];
      }
    }
  }

  // Update weights of all outgoing edges
  for (auto &[edge, rti] : rti_map) {
    V v = prop.operation_to_vertex[{edge, ms}];
    for (E vw :
         boost::make_iterator_range(boost::out_edges(v, shuffle_graph))) {
      if (shuffle_graph[vw].edge_type != conjunctive)
        continue;
      V w = target(vw, shuffle_graph);
      bool w_contains_ms = std::find(shuffle_graph[w].ms_handle.begin(),
                                     shuffle_graph[w].ms_handle.end(),
                                     ms) != shuffle_graph[w].ms_handle.end();
      if (w_contains_ms) {
        Delay d_max_old = std::accumulate(
            shuffle_graph[v].ms_handle.begin(),
            shuffle_graph[v].ms_handle.end(), (Delay)0,
            [&](Delay d_max, auto ms1) {
              if (ms == ms1)
                return d_max;
              return std::max(d_max, prop.streams[ms1].rti_map[edge].d_max());
            });
        if (rti_map[edge].d_max() > d_max_old) {
          d_max_old =
              std::max(d_max_old, prop.streams[ms].rti_map[edge].d_max());
          shuffle_graph[vw].weight += rti_map[edge].d_max() - d_max_old;
        } else {
          Edge n_edge = shuffle_graph[w].edge;
          Delay delta_old = std::max(
              (Delay)0, prop.streams[ms].rti_map[edge].d_trans_max() -
                            prop.streams[ms].rti_map[n_edge].d_trans_min());
          Delay delta_new;
          if (rti_map.contains(n_edge)) {
            delta_new = std::max((Delay)0, rti_map[edge].d_trans_max() -
                                               rti_map[n_edge].d_trans_min());
          } else {
            delta_new = std::max(
                (Delay)0, rti_map[edge].d_trans_max() -
                              prop.streams[ms].rti_map[n_edge].d_trans_min());
          }
          shuffle_graph[vw].weight += delta_new - delta_old;
        }
      } else {
        shuffle_graph[vw].weight +=
            rti_map[edge].d_trans_max() -
            prop.streams[ms].rti_map[edge].d_trans_max();
      }
    }
  }

  // Update weights of all ingoing FIFO edges
  for (auto &[edge, rti] : rti_map) {
    V v = prop.operation_to_vertex[{edge, ms}];
    for (E uv : boost::make_iterator_range(boost::in_edges(v, shuffle_graph))) {
      if (shuffle_graph[uv].edge_type == fifo) {
        Delay d_min_old = std::accumulate(
            shuffle_graph[v].ms_handle.begin(),
            shuffle_graph[v].ms_handle.end(), std::numeric_limits<Delay>::max(),
            [&](Delay d_min, auto ms1) {
              if (ms == ms1)
                return d_min;
              return std::min(d_min, prop.streams[ms1].rti_map[edge].d_min());
            });

        if (rti_map[edge].d_min() < d_min_old) {
          d_min_old =
              std::min(d_min_old, prop.streams[ms].rti_map[edge].d_min());
          shuffle_graph[uv].weight -= rti_map[edge].d_min() - d_min_old;
        }
      }
    }
  }

  // Update stored rti_maps
  for (auto &[edge, rti] : rti_map) {
    prop.streams[ms].rti_map[edge] = rti_map[edge];
  }
  prop.streams[ms].initialize();
}

void DisjunctiveGraphModel::internal_commit_all(size_t index) {
  std::map<V, V> updates;
  reversed_dgm_traversal(
      shuffle_graph,
      visitor(update_machine_successors_visitor(shuffle_graph, updates))
          .root_vertex(shuffle_graph[boost::graph_bundle].sink));
  update_machine_successors(updates);
  commit_flips();

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

void DisjunctiveGraphModel::internal_copy_commit(size_t src_index,
                                                 size_t dst_index) {
  for (size_t i = committed_shuffle_graphs.size(); i <= dst_index; i++)
    committed_shuffle_graphs.push_back(shuffle_graph_t());

  committed_shuffle_graphs[dst_index].clear();
  boost::copy_graph(committed_shuffle_graphs[src_index],
                    committed_shuffle_graphs[dst_index]);
  committed_shuffle_graphs[dst_index][boost::graph_bundle] =
      committed_shuffle_graphs[src_index][boost::graph_bundle];

  for (E e : boost::make_iterator_range(boost::edges(shuffle_graph))) {
    if (shuffle_graph[e].edge_type == disjunctive) {
      shuffle_graph[e].state_pair->copy_commit(src_index, dst_index);
    }
  }
}

void DisjunctiveGraphModel::restore_flips(size_t n) {
  if (n == 0)
    return;

  for (int k = n; k > 0; k--, flip_log.pop_front()) {
    E uv = rev_edge(flip_log.front());
    shuffle_graph[uv].consistent_flip(shuffle_graph);
  }

  valid_crit_path = false;
}

void DisjunctiveGraphModel::internal_restore_commit(size_t index, bool swap) {
  flip_log.clear();

  if (index > committed_shuffle_graphs.size())
    throw std::runtime_error("commit does not exist");

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
  renew_descriptors();
  update_machine_successors();
}

void DisjunctiveGraphModel::update_machine_successors() {
  if (valid_crit_path)
    return;

  // ensure that processing_order is computed correctly
  std::map<V, V> updates;
  reversed_dgm_traversal(
      shuffle_graph,
      visitor(update_machine_successors_visitor(shuffle_graph, updates))
          .root_vertex(shuffle_graph[boost::graph_bundle].sink));
  update_machine_successors(updates);

  valid_crit_path = true;
}

void DisjunctiveGraphModel::update_machine_successors(std::map<V, V> updates) {
  for (auto &[u, v] : updates) {
    if (!shuffle_graph[u].MS.has_value() || shuffle_graph[u].MS->v == v)
      continue;

    V old_succ = shuffle_graph[u].MS->v;
    shuffle_graph[old_succ].MP = {};
    for (auto &old_succ_parent : shuffle_graph[old_succ].JP) {
      shuffle_graph[old_succ_parent.v].FP.erase(old_succ);
    }
  }
  for (auto &[u, v] : updates) {
    if (v != 0) {
      update_machine_successor(shuffle_graph, u, v);
    } else {
      shuffle_graph[u].MS = {};
      shuffle_graph[u].FS.clear();
    }
    shuffle_graph[u].neighbors_are_valid = true;
  }
}

std::vector<boost::graph_traits<shuffle_graph_t>::vertex_descriptor>
DisjunctiveGraphModel::get_processing_order(shuffle_graph_t &g, Edge edge) {
  auto &prop = g[boost::graph_bundle];
  MessageStreamHandle ms = *prop.edge_to_streams[edge].begin();
  return get_processing_order(g, prop.operation_to_vertex[{edge, ms}]);
}

std::vector<boost::graph_traits<shuffle_graph_t>::vertex_descriptor>
DisjunctiveGraphModel::get_processing_order(shuffle_graph_t &g, V v) {
  assert((g[v].neighbors_are_valid));

  std::vector<V> processing_order;
  for (auto u = g[v].MP.transform([&](auto &nv) { return nv.v; });
       u.has_value(); u = g[*u].MP.transform([&](auto &nv) { return nv.v; })) {
    assert((g[*u].neighbors_are_valid));
    processing_order.insert(processing_order.begin(), *u);
  }
  for (std::optional<V> u = v; u.has_value();
       u = g[*u].MS.transform([&](auto &nv) { return nv.v; })) {
    assert((g[*u].neighbors_are_valid));
    processing_order.push_back(*u);
  }

  return processing_order;
}

void DisjunctiveGraphModel::complete_flip(std::list<E> &edges, bool combined) {
  if (!combined) {
    for (E e : edges)
      complete_flip(e);
  } else {
    for (E &e : edges) {
      if (shuffle_graph[e].state() == blocked)
        e = rev_edge(e);
      else if (shuffle_graph[e].edge_type == fifo)
        e = fifo_to_disjunctive_edge(e);
    }

    std::set<OrientationState *> flipped_edges;
    std::set<V> shuffled_operations = {};
    size_t initial_flip_log_size = flip_log.size();
    for (E e : edges) {
      assert((shuffle_graph[e].edge_type != conjunctive));
      shuffle_graph[e].consistent_flip(shuffle_graph);
      flipped_edges.insert(shuffle_graph[e].state_pair->state.get());
      flip_log.push_front(e);
    }
    try {
      complete_flip(flipped_edges, shuffled_operations, {});
    } catch (FlipGraphException &e) {
      // restore initial state
      restore_flips(flip_log.size() - initial_flip_log_size);
      update_machine_successors();
      throw;
    }
  }
}

void DisjunctiveGraphModel::complete_flip(E e) {
  if (shuffle_graph[e].state() == blocked)
    e = rev_edge(e);
  else if (shuffle_graph[e].edge_type == fifo)
    e = fifo_to_disjunctive_edge(e);

  std::set<OrientationState *> flipped_edges;
  std::set<V> shuffled_operations = {};
  try {
    complete_flip(flipped_edges, shuffled_operations, e);
  } catch (FlipGraphException &e) {
    update_machine_successors();
    throw;
  }
}

void DisjunctiveGraphModel::complete_flip(
    std::set<OrientationState *> &flipped_edges,
    const std::set<V> &shuffled_operations, std::optional<E> uv) {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];

  auto find = std::find_if(flipped_edges.begin(), flipped_edges.end(),
                           [&](auto &state) { return *state == allowed; });
  if (find != flipped_edges.end()) {
    throw FlipGraphException(prop.equivalence_class_representative[*find]);
  }

  size_t initial_flip_log_size = flip_log.size();
  std::map<V, V> updated_machine_successors;

  // Simple shortcut that eliminates one additional DFS by eliminating trivial
  // cycles
  std::list<E> required_flips;
  std::set<OrientationState *> required_flips_classes;
  if (uv.has_value()) {
    required_flips.push_back(*uv);
    required_flips_classes.insert(shuffle_graph[*uv].state_pair->state.get());

    V u = source(*uv, shuffle_graph), v = target(*uv, shuffle_graph), w = u;
    while (w != v && shuffle_graph[w].neighbors_are_valid) {
      E wv = edge(w, v);
      bool new_flip = required_flips_classes
                          .insert(shuffle_graph[wv].state_pair->state.get())
                          .second;
      if (new_flip) {
        required_flips.push_back(wv);
      }
      w = shuffle_graph[w].MS->v;
    }
  }

  try {
    do {
      for (E e : required_flips) {
        assert((shuffle_graph[e].edge_type != conjunctive));
        shuffle_graph[e].consistent_flip(shuffle_graph);
        flipped_edges.insert(shuffle_graph[e].state_pair->state.get());
        flip_log.push_front(e);
      }
      required_flips.clear();

      total_flips++;
      reversed_dgm_traversal(
          shuffle_graph,
          visitor(complete_flip_visitor(shuffle_graph, flipped_edges,
                                        shuffled_operations, required_flips,
                                        updated_machine_successors))
              .root_vertex(prop.sink));
    } while (!required_flips.empty());
  } catch (FlipGraphException &e) {
    // restore initial state
    restore_flips(flip_log.size() - initial_flip_log_size);
    throw;
  }

  update_machine_successors(updated_machine_successors);

  // In the last iteration, required_flips is empty (i.e., there are no cycles).
  // Hence, we simultaneously compute the critical path (eliminating one
  // additional DFS)
  valid_crit_path = true;
}

void DisjunctiveGraphModel::complete_shuffle(const std::list<E> &edges,
                                             bool commit_fallback,
                                             bool fix_cycles) {
  if (commit_fallback)
    internal_commit_all(shuffle_fallback);
  try {
    for (E e : edges) {
      std::set<OrientationState *> flipped_edges;
      std::set<V> shuffled_operations;
      complete_shuffle(e, flipped_edges, shuffled_operations, fix_cycles);
    }
  } catch (std::exception &e) {
    internal_restore_commit(shuffle_fallback, false);
    throw;
  }
  shuffle_graph[boost::graph_bundle].is_zips_selection = false;
  if (fix_cycles)
    renew_descriptors();
}

void DisjunctiveGraphModel::complete_shuffle(E e, bool commit_fallback,
                                             bool fix_cycles) {
  if (commit_fallback)
    internal_commit_all(shuffle_fallback);
  std::set<OrientationState *> flipped_edges;
  std::set<V> shuffled_operations;
  try {
    complete_shuffle(e, flipped_edges, shuffled_operations, fix_cycles);
  } catch (std::exception &e) {
    internal_restore_commit(shuffle_fallback, false);
    throw;
  }
  shuffle_graph[boost::graph_bundle].is_zips_selection = false;
  if (fix_cycles)
    renew_descriptors();
}

void DisjunctiveGraphModel::complete_shuffle(
    E uv, std::set<OrientationState *> &flipped_edges,
    std::set<V> &shuffled_operations, bool fix_cycles) {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];

  if (shuffle_graph[uv].state() == blocked)
    uv = rev_edge(uv);
  else if (shuffle_graph[uv].edge_type == fifo)
    uv = fifo_to_disjunctive_edge(uv);

  bool closed = false;
  while (!closed) {
    closed = true;
    auto &equivalence_class = shuffle_graph[uv].state_pair->equivalence_class;
    for (auto &[u, v] : equivalence_class) {
      E uv = edge(u, v), uw;
      if (shuffle_graph[uv].edge_type != disjunctive)
        continue;

      auto violation = std::find_if(
          equivalence_class.begin(), equivalence_class.end(), [&](auto &e) {
            return v == e.first && !equivalence_class.contains({u, e.second});
          });
      if (violation != equivalence_class.end()) {
        uw = edge(u, (*violation).second);
      } else {
        violation = std::find_if(
            equivalence_class.begin(), equivalence_class.end(), [&](auto &e) {
              auto uw = boost::edge(u, e.first, shuffle_graph);
              if (!uw.second ||
                  shuffle_graph[uw.first].edge_type != disjunctive)
                return false;
              return v == e.second &&
                     !equivalence_class.contains({u, e.first}) &&
                     !equivalence_class.contains({e.first, u});
            });
        if (violation != equivalence_class.end()) {
          uw = edge(u, (*violation).first);
        } else {
          continue;
        }
      }

      flipped_edges.erase(shuffle_graph[uw].state_pair->state.get());
      flipped_edges.erase(shuffle_graph[uw].state_pair->reversed_state.get());
      shuffle_graph[uw].merge_equivalence_classes(uv, shuffle_graph, prop);
      shuffle_graph[rev_edge(uw)].merge_equivalence_classes(
          rev_edge(uv), shuffle_graph, prop);
      closed = false;
      break;
    }
  }

  // Remove FIFO edges induced by (u,v) and shuffle the operations
  std::set<MessageStreamHandle> affected_streams;
  for (auto &[u, v] : shuffle_graph[uv].state_pair->equivalence_class) {
    if (shuffle_graph[u].edge != shuffle_graph[v].edge) {
      std::erase_if(shuffle_graph[v].FP, [&](const auto &item) {
        auto const &[w, nv] = item;
        return shuffle_graph[uv].state_pair->equivalence_class.contains({u, w});
      });
      continue;
    }
    remove_fifo_edges(u, v);
    remove_fifo_edges(v, u);
    for (MessageStreamHandle ms_handle : shuffle_graph[v].ms_handle) {
      prop.operation_to_vertex[Operation(shuffle_graph[v].edge, ms_handle)] = u;
    }
    shuffle_graph[u].shuffle(shuffle_graph, shuffle_graph[v]);
    shuffled_operations.insert(u);
    for (MessageStreamHandle ms_handle : shuffle_graph[u].ms_handle) {
      affected_streams.insert(ms_handle);
    }
  }

  for (auto &[u, v] : shuffle_graph[uv].state_pair->equivalence_class) {
    if (shuffle_graph[u].edge != shuffle_graph[v].edge)
      continue;

    shuffle_graph[u].invalidate_neighbors(shuffle_graph);
    if (shuffle_graph[u].MP.has_value())
      shuffle_graph[shuffle_graph[u].MP->v].invalidate_neighbors(shuffle_graph);
    if (shuffle_graph[v].MS.has_value())
      shuffle_graph[shuffle_graph[v].MS->v].invalidate_neighbors(shuffle_graph);

    // Update outgoing conjunctive edges by updating their weights
    // along with JS and JP
    for (auto &[w, vw] : conjunctive_out_edges(v, shuffle_graph)) {
      if (w != prop.sink && shuffle_graph[w].ms_handle.empty())
        continue;

      Edge edge = shuffle_graph[u].edge;
      Edge n_edge = shuffle_graph[w].edge;
      NeighborVertex uw;
      auto u_JS = std::find_if(shuffle_graph[u].JS.begin(),
                               shuffle_graph[u].JS.end(), [&](auto &nv) {
                                 return nv.v == w || shuffle_graph[nv.v].edge ==
                                                         shuffle_graph[w].edge;
                               });
      if (u_JS != shuffle_graph[u].JS.end()) {
        if (w == prop.sink)
          shuffle_graph[w].JP.remove({v, vw});

        uw = *u_JS;
      } else {
        assert((!boost::edge(u, w, shuffle_graph).second));

        uw = {w, boost::add_edge(u, w, shuffle_graph[vw], shuffle_graph).first};
        shuffle_graph[w].JP.remove({v, vw});
        shuffle_graph[u].JS.push_back({w, uw.e});
        shuffle_graph[w].JP.push_back({u, uw.e});
      }
    }

    for (auto &[w, uw] : conjunctive_out_edges(u, shuffle_graph)) {
      Edge edge = shuffle_graph[u].edge;
      Edge n_edge = shuffle_graph[w].edge;
      shuffle_graph[uw].weight = 0;
      if (network->has_multiple_subcarriers(edge)) {
        Delay d_wireline = 0;
        for (auto &handle : shuffle_graph[u].ms_handle) {
          shuffle_graph[uw].weight =
              std::max(shuffle_graph[uw].weight,
                       prop.streams[handle].rti_map[edge].d_max());
          d_wireline += prop.streams[handle].rti_map[edge].d_wireline();
        }
        shuffle_graph[uw].weight += d_wireline;
      } else {
        std::pair<Delay, Delay> max_delay = {0, 0};
        for (auto &handle : shuffle_graph[u].ms_handle) {
          if (std::find(shuffle_graph[w].ms_handle.begin(),
                        shuffle_graph[w].ms_handle.end(),
                        handle) != shuffle_graph[w].ms_handle.end() &&
              !network->has_multiple_subcarriers(n_edge)) {
            std::pair<Delay, Delay> delay = {
                prop.streams[handle].rti_map[edge].d_max(),
                std::max(prop.streams[handle].rti_map[edge].d_trans_max() -
                             prop.streams[handle].rti_map[n_edge].d_trans_min(),
                         (Delay)0)};
            if (delay.first > max_delay.first)
              max_delay = delay;
            // (edge, handle) is shuffled with w
            shuffle_graph[uw].weight += delay.second;
          } else {
            // (edge, handle) is not shuffled with w
            shuffle_graph[uw].weight +=
                prop.streams[handle].rti_map[edge].d_trans_max();
          }
        }
        shuffle_graph[uw].weight += max_delay.first - max_delay.second;
      }
    }

    // Analogously, we need to update the *first* conjunctive edges that are
    // redirected to the newly shuffled operations.
    // All other incoming edges are already handled by conjunctive_out_edges.
    for (auto &[w, wv] : conjunctive_in_edges(v, shuffle_graph)) {
      if (w != prop.src && shuffle_graph[w].ms_handle.empty())
        continue;

      auto u_JP = std::find_if(shuffle_graph[u].JP.begin(),
                               shuffle_graph[u].JP.end(), [&](auto &nv) {
                                 return nv.v == w || shuffle_graph[nv.v].edge ==
                                                         shuffle_graph[w].edge;
                               });
      if (u_JP == shuffle_graph[u].JP.end()) {
        assert((!boost::edge(w, u, shuffle_graph).second));

        E wu = boost::add_edge(w, u, shuffle_graph[wv], shuffle_graph).first;
        shuffle_graph[u].JP.push_back({w, wu});
        shuffle_graph[w].JS.push_back({u, wu});
        shuffle_graph[w].JS.remove({v, wv});
        shuffle_graph[w].FP.erase(v);
      } else if (w == prop.src) {
        shuffle_graph[w].JS.remove({v, wv});
      }
    }

    // Update the outgoing disjunctive and FIFO edges by merging the
    // equivalence classes of vw and uw and adjusting the weights.
    // With MS[v] = u, we do not need to update the machine
    // successors/predecessors here.
    for (E vw :
         boost::make_iterator_range(boost::out_edges(v, shuffle_graph))) {
      V w = target(vw, shuffle_graph);
      if (w == u || shuffle_graph[vw].edge_type == conjunctive)
        continue;

      auto [uw, exists] = boost::edge(u, w, shuffle_graph);
      if (!exists || shuffle_graph[vw].relates_to(shuffle_graph[uw]) ||
          shuffle_graph[vw].relates_to(shuffle_graph[rev_edge(uw)]))
        continue;

      shuffle_graph[uw].weight +=
          shuffle_graph[fifo_to_disjunctive_edge(vw)].weight;
    }

    // Updating the incoming disjunctive and FIFO edges is more complex, as we
    // need to update the machine successors/predecessors here as well.
    for (E wv : boost::make_iterator_range(boost::in_edges(v, shuffle_graph))) {
      V w = source(wv, shuffle_graph);
      if (shuffle_graph[uv].relates_to(shuffle_graph[wv]) ||
          shuffle_graph[wv].edge_type != fifo)
        continue;

      // FIFO edge w -> u does not necessarily exist. Thus, add this edge if
      // needed and adjust the weights accordingly.
      auto wu = boost::edge(w, u, shuffle_graph);
      if (!wu.second) {
        wu = boost::add_edge(w, u, shuffle_graph[wv], shuffle_graph);
        shuffle_graph[wu.first].state_pair->add_edge(wu.first, shuffle_graph);
      } else {
        shuffle_graph[wu.first].weight =
            std::min(shuffle_graph[wu.first].weight, shuffle_graph[wv].weight);
      }

      // Update FS[w] and FP[u]
      auto u_JS = std::find_if(shuffle_graph[u].JS.begin(),
                               shuffle_graph[u].JS.end(), [&](auto &nv) {
                                 return nv.v == w || shuffle_graph[nv.v].edge ==
                                                         shuffle_graph[w].edge;
                               });
      if (shuffle_graph[w].FS.remove_if(
              [v](NeighborVertex &nv) { return nv.v == v; }) > 0) {
        assert((u_JS != shuffle_graph[u].JS.end() && w != prop.sink));

        shuffle_graph[w].FS.push_back({u, wu.first});
        shuffle_graph[u].FP[(*u_JS).v] = {w, wu.first};
      }
    }
  }

  for (auto &[u, v] : shuffle_graph[uv].state_pair->equivalence_class) {
    for (E vw :
         boost::make_iterator_range(boost::out_edges(v, shuffle_graph))) {
      V w = target(vw, shuffle_graph);
      if (w == u || shuffle_graph[vw].edge_type != disjunctive)
        continue;

      auto [uw, exists] = boost::edge(u, w, shuffle_graph);
      if (!exists || shuffle_graph[vw].relates_to(shuffle_graph[uw]) ||
          shuffle_graph[vw].relates_to(shuffle_graph[rev_edge(uw)]))
        continue;

      // replace vw (which will be invalid after merging) with uw in
      // flipped_edges (if vw is contained in flipped_edges)
      if (flipped_edges.erase(shuffle_graph[vw].state_pair->state.get()) > 0)
        flipped_edges.insert(shuffle_graph[uw].state_pair->state.get());

      if (shuffle_graph[vw].state() != shuffle_graph[uw].state()) {
        if (shuffle_graph[uw].state() == blocked)
          shuffle_graph[rev_edge(uw)].consistent_flip(shuffle_graph);
        else
          shuffle_graph[rev_edge(vw)].consistent_flip(shuffle_graph);

        // remove vw from flipped_edges (if exists) and add uw either way
        flipped_edges.erase(shuffle_graph[vw].state_pair->reversed_state.get());
        flipped_edges.insert(
            shuffle_graph[uw].state_pair->reversed_state.get());
      }

      shuffle_graph[vw].merge_equivalence_classes(uw, shuffle_graph, prop);
    }

    for (E wv : boost::make_iterator_range(boost::in_edges(v, shuffle_graph))) {
      V w = source(wv, shuffle_graph);
      if (shuffle_graph[uv].relates_to(shuffle_graph[wv]) ||
          shuffle_graph[wv].edge_type != disjunctive)
        continue;

      auto [wu, exists] = boost::edge(w, u, shuffle_graph);
      if (!exists || shuffle_graph[wv].relates_to(shuffle_graph[wu]) ||
          shuffle_graph[wv].relates_to(shuffle_graph[rev_edge(wu)]))
        continue;

      // replace wv (which will be invalid after merging) with wu in
      // flipped_edges (if wv is contained in flipped_edges)
      if (flipped_edges.erase(shuffle_graph[wv].state_pair->state.get()) > 0)
        flipped_edges.insert(shuffle_graph[wu].state_pair->state.get());
      if (flipped_edges.erase(
              shuffle_graph[wv].state_pair->reversed_state.get()) > 0)
        flipped_edges.insert(
            shuffle_graph[wu].state_pair->reversed_state.get());

      shuffle_graph[wv].merge_equivalence_classes(wu, shuffle_graph, prop);
    }
  }

  // remove uv from flipped_edges
  flipped_edges.erase(shuffle_graph[uv].state_pair->state.get());
  flipped_edges.erase(shuffle_graph[uv].state_pair->reversed_state.get());

  // remove edges but do not remove vertex to avoid invalidating vertex and
  // edge descriptors
  for (auto &[u, v] : shuffle_graph[uv].state_pair->equivalence_class) {
    if (shuffle_graph[u].edge == shuffle_graph[v].edge) {
      // Normally, erasing MP and FP of u and w would be handled by
      // complete_flip. Here, however, we would lose the information by clearing
      // v. Hence, we handle it manually here.
      if (!shuffle_graph[v].MP.has_value() || shuffle_graph[v].MP->v != u) {
        shuffle_graph[u].MP = {};
        for (auto &u_parent : shuffle_graph[u].JP) {
          shuffle_graph[u_parent.v].FP.erase(u);
        }
      }
      if (shuffle_graph[v].MS.has_value() && shuffle_graph[v].MS->v != u) {
        V w = shuffle_graph[v].MS->v;
        shuffle_graph[w].MP = {};
        for (auto &w_parent : shuffle_graph[w].JP) {
          shuffle_graph[w_parent.v].FP.erase(w);
        }
      }

      boost::clear_vertex(v, shuffle_graph);
      shuffle_graph[v].clear();
    }
  }

  // check jitter bounds
  for (auto ms : affected_streams) {
    auto max_jitter = compute_jitter_bound(ms);
    if (max_jitter.first > prop.streams[ms].jitter)
      throw JitterBoundViolation(ms, max_jitter.second,
                                 prop.streams[ms].jitter);
  }

  // remove deleted edges from equivalence classes
  for (E e : boost::make_iterator_range(boost::edges(shuffle_graph))) {
    std::erase_if(shuffle_graph[e].state_pair->equivalence_class, [&](auto uv) {
      auto &[u, v] = uv;
      return !boost::edge(u, v, shuffle_graph).second;
    });
  }

  if (fix_cycles) {
    bool complete = false;
    while (!complete) {
      try {
        // complete_flip should not modify flipped_edges here
        auto flipped_edges_copy = flipped_edges;
        complete_flip(flipped_edges_copy, shuffled_operations, {});
        complete = true;
      } catch (FlipGraphException &e) {
        // update_machine_successors();
        complete_shuffle(edge(e.required_shuffle), flipped_edges,
                         shuffled_operations);
      }
    }
  }

  flip_log.clear();
}

void DisjunctiveGraphModel::split_all() {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  ShuffleGraphProperty &z_prop =
      committed_shuffle_graphs[0][boost::graph_bundle];

  // get processing order of current graph
  std::map<Edge, std::vector<V>> processing_order;
  for (auto &[edge, streams] : prop.edge_to_streams) {
    std::optional<V> u;
    for (auto &s : streams) {
      u = prop.operation_to_vertex[Operation(edge, s)];
      if (!shuffle_graph[*u].MP.has_value())
        break;
    }

    processing_order[edge] = {};
    while (u.has_value()) {
      for (auto &s : shuffle_graph[*u].ms_handle) {
        processing_order[edge].push_back(
            z_prop.operation_to_vertex[Operation(edge, s)]);
      }

      u = shuffle_graph[*u].MS.has_value() ? shuffle_graph[*u].MS->v
                                           : std::optional<V>();
    }
  }

  // Copy initial_shuffle_graph to shuffle_graph and reset equivalence
  // classes.
  internal_restore_commit(initial, false);
  apply_processing_order(processing_order);
}

std::pair<Delay, Edge>
DisjunctiveGraphModel::compute_jitter_bound(MessageStreamHandle ms) {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];

  Delay max_jitter = 0;
  Edge edge;
  for (auto &listener : prop.streams[ms].route->get_listeners()) {
    Delay jitter = compute_jitter(ms, listener);
    if (jitter > max_jitter) {
      edge = listener;
      max_jitter = jitter;
    }
  }

  return {max_jitter, edge};
}

Delay DisjunctiveGraphModel::compute_jitter(MessageStreamHandle ms,
                                            Edge listener) {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  V v_listener = prop.operation_to_vertex[{listener, ms}];

  // at worst (best), ms is transmitted last (first).
  Delay jitter =
      std::accumulate(shuffle_graph[v_listener].ms_handle.begin(),
                      shuffle_graph[v_listener].ms_handle.end(), (Delay)0,
                      [&](Delay dmax, auto ms1) {
                        return dmax +
                               prop.streams[ms1].rti_map[listener].d_max();
                      }) -
      prop.streams[ms].rti_map[listener].d_min();
  return jitter;
}

bool DisjunctiveGraphModel::apriori_jitter_violation(E e) {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  for (auto [u, v] : shuffle_graph[e].state_pair->equivalence_class) {
    E uv = edge(u, v);
    if (shuffle_graph[uv].edge_type == fifo)
      continue;

    for (auto u_ms : shuffle_graph[u].ms_handle) {
      for (auto &u_ms_listener : prop.streams[u_ms].route->get_listeners()) {
        Delay u_jitter = compute_jitter(u_ms, u_ms_listener);
        for (auto v_ms : shuffle_graph[v].ms_handle) {
          for (auto &v_ms_listener :
               prop.streams[v_ms].route->get_listeners()) {
            if (u_ms_listener != v_ms_listener)
              continue;
            Delay v_jitter = compute_jitter(v_ms, v_ms_listener);
            if (u_jitter + v_jitter +
                        prop.streams[v_ms].rti_map[v_ms_listener].d_min() >
                    prop.streams[u_ms].jitter ||
                u_jitter + v_jitter +
                        prop.streams[u_ms].rti_map[u_ms_listener].d_min() >
                    prop.streams[v_ms].jitter)
              return true;
          }
        }
      }
    }
  }
  return false;
}

void DisjunctiveGraphModel::remove_fifo_edges(V u, V v) {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];

  for (auto &nv : shuffle_graph[v].JP) {
    auto search = boost::edge(u, nv.v, shuffle_graph);
    if (search.second) {
      boost::remove_edge(search.first, shuffle_graph);
      auto search = shuffle_graph[nv.v].FP.find(v);
      if (search != shuffle_graph[nv.v].FP.end() && search->second.v == u) {
        shuffle_graph[nv.v].FP.erase(v);
      }
    }
  }
}

void DisjunctiveGraphModel::build() {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  std::ranges::sort(prop.streams,
                    [&](const MessageStream &s1, const MessageStream &s2) {
                      return s1.phase > s2.phase;
                    });

  prop.hyperperiod = 1;
  for (MessageStreamHandle i = 0; i < prop.streams.size(); i++) {
    build_stream(i);
    prop.hyperperiod = std::lcm(prop.hyperperiod, prop.streams[i].period);
  }
  resize_properties();

  internal_commit_all(initial);
}

void DisjunctiveGraphModel::resize_properties() {
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  prop.crit_cost.resize(boost::num_vertices(shuffle_graph));
  prop.crit_pred.resize(boost::num_vertices(shuffle_graph));
  prop.cycle_pred.resize(boost::num_vertices(shuffle_graph));
  prop.slack.resize(boost::num_vertices(shuffle_graph));
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
      E e = boost::add_edge(
                v_parent, v,
                {prop.streams[handle].rti_map[hop.parent->edge].d_max(),
                 CONJUNCTIVE_STATE, conjunctive},
                shuffle_graph)
                .first;
      shuffle_graph[v].JP = {{v_parent, e}};
      shuffle_graph[v_parent].JS.push_back({v, e});
    } else {
      E e = boost::add_edge(
                prop.src, v,
                {prop.streams[handle].phase, CONJUNCTIVE_STATE, conjunctive},
                shuffle_graph)
                .first;
      shuffle_graph[v].JP = {{prop.src, e}};
      shuffle_graph[prop.src].JS.push_back({v, e});
    }

    // add edge to sink
    if (hop.is_leaf()) {
      E e = boost::add_edge(v, prop.sink,
                            {prop.streams[handle].rti_map[hop.edge].d_max(),
                             CONJUNCTIVE_STATE, conjunctive},
                            shuffle_graph)
                .first;
      shuffle_graph[v].JS = {{prop.sink, e}};
      shuffle_graph[prop.sink].JP.push_back({v, e});
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

        V v_parent, u_parent;
        E fvu, fuv;
        // add FIFO edges u -> v
        if (!hop.parent->is_root()) {
          Delay weight =
              (network->has_multiple_subcarriers(hop.edge)
                   ? prop.streams[other].rti_map[hop.edge].d_wireline()
                   : prop.streams[other].rti_map[hop.edge].d_trans_max()) -
              prop.streams[handle].rti_map[v_hop_parent->edge].d_min();
          v_parent = prop.operation_to_vertex[{v_hop_parent->edge, handle}];
          fuv = boost::add_edge(u, v_parent, {weight, states.second, fifo},
                                shuffle_graph)
                    .first;
          shuffle_graph[fuv].state_pair->add_edge(fuv, shuffle_graph);
        }

        // add FIFO edges v -> u
        if (!u_hop_parent->is_root()) {
          Delay weight =
              (network->has_multiple_subcarriers(hop.edge)
                   ? prop.streams[handle].rti_map[hop.edge].d_wireline()
                   : prop.streams[handle].rti_map[hop.edge].d_trans_max()) -
              prop.streams[other].rti_map[u_hop_parent->edge].d_min();
          u_parent = prop.operation_to_vertex[{u_hop_parent->edge, other}];
          fvu = boost::add_edge(v, u_parent, {weight, states.first, fifo},
                                shuffle_graph)
                    .first;
          shuffle_graph[fvu].state_pair->add_edge(fvu, shuffle_graph);
        }

        // add disjunctive edge v -> u
        Delay weight =
            network->has_multiple_subcarriers(hop.edge)
                ? prop.streams[handle].rti_map[hop.edge].d_wireline()
                : prop.streams[handle].rti_map[hop.edge].d_trans_max();
        E vu = boost::add_edge(v, u, {weight, states.first, disjunctive},
                               shuffle_graph)
                   .first;
        shuffle_graph[vu].state_pair->add_edge(vu, shuffle_graph);
        prop.equivalence_class_representative[states.first->state.get()] = vu;

        // add disjunctive edge u -> v
        weight = network->has_multiple_subcarriers(hop.edge)
                     ? prop.streams[other].rti_map[hop.edge].d_wireline()
                     : prop.streams[other].rti_map[hop.edge].d_trans_max();
        E uv = boost::add_edge(u, v, {weight, states.second, disjunctive},
                               shuffle_graph)
                   .first;
        shuffle_graph[uv].state_pair->add_edge(uv, shuffle_graph);
        prop.equivalence_class_representative[states.second->state.get()] = uv;

        if (!shuffle_graph[u].MP.has_value()) {
          shuffle_graph[v].MS = {u, vu};
          shuffle_graph[u].MP = {v, vu};

          if (!u_hop_parent->is_root()) {
            shuffle_graph[v].FS.push_back({u_parent, fvu});
            shuffle_graph[u_parent].FP[u] = {v, fvu};
          }
        }
      }
      edge_search->second.insert(handle);
    }
  }
}

void DisjunctiveGraphModel::encode(std::vector<unsigned int> &buf,
                                   shuffle_graph_t &g, OffsetMap &offset_map) {
  auto &prop = g[boost::graph_bundle];
  buf.clear();
  offset_map.clear();

  for (auto &[e, streams] : prop.edge_to_streams) {
    offset_map[e] = buf.end() - buf.begin();
    buf.insert(buf.end(), {MACHINE_SEPARATOR, e.first, e.second});

    auto processing_order = get_processing_order(g, e);
    for (V v : processing_order) {
      if (g[v].ms_handle.size() > 1) {
        buf.push_back(SHUFFLE_SEPARATOR);
        for (auto ms : g[v].ms_handle)
          buf.push_back(ms);
        buf.push_back(SHUFFLE_SEPARATOR);
      } else {
        buf.push_back(g[v].ms_handle.front());
      }
    }
  }
  buf.push_back(MACHINE_SEPARATOR);
}

void DisjunctiveGraphModel::decode(std::vector<unsigned int> &buf) {
  auto &prop = shuffle_graph[boost::graph_bundle];

  std::map<Edge, std::vector<V>> processing_order;

  internal_restore_commit(initial, false);

  // compute processing order, and shuffle operations
  int offset = 0, len;
  while (offset + 1 < buf.size()) {
    if (buf[offset] != MACHINE_SEPARATOR)
      throw std::runtime_error("invalid DGM encoding");

    Edge edge = {buf[offset + 1], buf[offset + 2]};
    processing_order[edge] = {};
    len = 3;

    while (buf[offset + len] != MACHINE_SEPARATOR) {
      if (buf[offset + len] == SHUFFLE_SEPARATOR) {
        MessageStreamHandle v_ms = buf[offset + len + 1];
        V v = prop.operation_to_vertex[{edge, v_ms}];
        len += 2;
        while (buf[offset + len] != SHUFFLE_SEPARATOR) {
          MessageStreamHandle u_ms = buf[offset + len];
          V u = prop.operation_to_vertex[{edge, u_ms}];
          if (u != v) {
            complete_shuffle(this->edge(v, u), false, false);
            if (shuffle_graph[v].ms_handle.empty())
              v = u;
          }
          len++;
        }
        processing_order[edge].push_back(v);
      } else {
        MessageStreamHandle ms = buf[offset + len];
        processing_order[edge].push_back(prop.operation_to_vertex[{edge, ms}]);
      }
      len++;
    }

    offset += len;
  }

  apply_processing_order(processing_order);
  valid_crit_path = false;
  update_machine_successors();
}

// Precondition: operations contains all vertices of the machine
void DisjunctiveGraphModel::apply_machine_processing_order(
    const std::vector<V> &operations) {
  assert((std::accumulate(operations.begin(), operations.end(), 0,
                          [&](size_t s, V v) {
                            return s + shuffle_graph[v].ms_handle.size();
                          }) ==
          shuffle_graph[boost::graph_bundle]
              .edge_to_streams[shuffle_graph[operations.front()].edge]
              .size()));

  V u = operations[0];
  shuffle_graph[u].MP = {};
  for (auto &u_parent : shuffle_graph[u].JP)
    shuffle_graph[u_parent.v].FP.erase(u);

  for (int i = 0; i < operations.size(); i++) {
    u = operations[i];
    for (int j = i + 1; j < operations.size(); j++) {
      V v = operations[j];
      E e = this->edge(u, v);
      if (shuffle_graph[e].state() == blocked)
        std::swap(*shuffle_graph[e].state_pair->state,
                  *shuffle_graph[e].state_pair->reversed_state);
      if (j == i + 1)
        update_machine_successor(shuffle_graph, u, v);
    }
    shuffle_graph[u].neighbors_are_valid = true;
  }

  shuffle_graph[u].MS = {};
  shuffle_graph[u].FS.clear();
}

void DisjunctiveGraphModel::renew_descriptors() {
  for (V v : boost::make_iterator_range(boost::vertices(shuffle_graph))) {
    auto &v_prop = shuffle_graph[v];
    if (v_prop.MP.has_value())
      v_prop.MP = {v_prop.MP.value().v, edge(v_prop.MP.value().e)};
    if (v_prop.MS.has_value())
      v_prop.MS = {v_prop.MS.value().v, edge(v_prop.MS.value().e)};

    for (auto &nv : v_prop.JP)
      nv.e = edge(nv.e);
    for (auto &nv : v_prop.JS)
      nv.e = edge(nv.e);
    for (auto &nv : v_prop.FS)
      nv.e = edge(nv.e);

    for (auto &[u, nv] : v_prop.FP)
      nv.e = edge(nv.e);
  }
}

} // namespace tsndgm
