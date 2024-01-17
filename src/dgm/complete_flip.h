#ifndef TSN_DGM_COMPLETE_FLIP_H
#define TSN_DGM_COMPLETE_FLIP_H

#include "critical_path.h"

namespace tsndgm {

class FlipGraphException : public std::exception {
public:
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  FlipGraphException(E required_shuffle) : required_shuffle(required_shuffle) {}
  const char *what() { return "flip graph violation detected"; }

  E required_shuffle;
};

class IncompleteSelectionException : public std::exception {
public:
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  IncompleteSelectionException() {}
  const char *what() {
    return "shuffle graph was acyclic before complete flip";
  }
};

class UnfixableCycleException : public std::exception {
public:
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  UnfixableCycleException() {}
  const char *what() {
    return "shuffle graph contains cycle without disjunctive edges";
  }
};

class complete_flip_visitor : public update_machine_successors_visitor {
public:
  complete_flip_visitor(shuffle_graph_t &shuffle_graph,
                        const std::set<OrientationState *> &flipped_edges,
                        const std::set<V> &shuffled_operations,
                        std::list<E> &required_flips,
                        std::map<V, V> &updated_machine_successors)
      : update_machine_successors_visitor(shuffle_graph,
                                          updated_machine_successors),
        flipped_edges(flipped_edges), shuffled_operations(shuffled_operations),
        required_flips(required_flips) {}

  void tree_edge(E e, const shuffle_graph_t &shuffle_graph) const {
    prop.cycle_pred[source(e, shuffle_graph)] = target(e, shuffle_graph);
  }

  void add_required_flips(E uv, const shuffle_graph_t &shuffle_graph) {
    V u = source(uv, shuffle_graph), v = target(uv, shuffle_graph);
    V w = prop.cycle_pred[v];
    do {
      auto wv = boost::edge(w, v, shuffle_graph);
      if (wv.second && shuffle_graph[wv.first].state() == allowed) {
        bool new_flip =
            required_flips_classes
                .insert(shuffle_graph[wv.first].state_pair->state.get())
                .second;
        if (new_flip)
          required_flips.push_back(wv.first);
      }
      w = prop.cycle_pred[w];
    } while (w != v);
  }

  bool back_edge(E uv, const shuffle_graph_t &shuffle_graph) {
    V u = source(uv, shuffle_graph), v = target(uv, shuffle_graph), w = v;

    V old_pred = prop.cycle_pred[u];
    prop.cycle_pred[u] = v;

    // if cycle contains edge to be flipped, there's nothing to do
    do {
      E e = boost::edge(w, prop.cycle_pred[w], shuffle_graph).first;

      if (required_flips_classes.contains(
              shuffle_graph[e].state_pair->state.get())) {
        prop.cycle_pred[u] = old_pred;
        return false;
      }

      w = prop.cycle_pred[w];
    } while (w != v);

    std::optional<E> last_disjunctive_edge = {};
    std::optional<E> breaking_edge = {};
    std::optional<V> breaking_operation = {};

    do {
      E e = boost::edge(w, prop.cycle_pred[w], shuffle_graph).first;
      w = prop.cycle_pred[w];

      // if cycle contains newly shuffled operation, last_disjunctive_edge
      // is flipped next
      if (shuffled_operations.contains(w)) {
        if (last_disjunctive_edge.has_value()) {
          add_required_flips(*last_disjunctive_edge, shuffle_graph);
          prop.cycle_pred[u] = old_pred;
          return false;
        } else if (!breaking_operation.has_value()) {
          breaking_operation = w;
        } else if (breaking_operation == w) {
          // cycle contains shuffled operations but no disjunctive edge
          throw UnfixableCycleException();
        }
      }

      // conjunctive edges cannot be flipped, continue
      if (shuffle_graph[e].edge_type == conjunctive) {
        continue;
      }

      if (!flipped_edges.contains(
              shuffle_graph[e].state_pair->reversed_state.get())) {
        // store edge that is eligible for flipping
        last_disjunctive_edge = e;
      } else {
        if (last_disjunctive_edge.has_value()) {
          add_required_flips(*last_disjunctive_edge, shuffle_graph);
          prop.cycle_pred[u] = old_pred;
          return false;
        }
        if (!breaking_edge.has_value()) {
          breaking_edge = e;
        } else {
          // requires that breaking_edge is shuffled
          throw FlipGraphException(*breaking_edge);
        }
      }
    } while (w != v || breaking_edge.has_value() ||
             breaking_operation.has_value());

    if (last_disjunctive_edge.has_value()) {
      // cycle contains at least one disjunctive edge (none of which was flipped
      // by the current operation) but no shuffled operation
      throw IncompleteSelectionException();
    } else {
      // cycle contains no disjunctive edges and no shuffled operations; hence
      // there must be some message stream with a cyclic route
      throw UnfixableCycleException();
    }
  }

  const std::set<OrientationState *> &flipped_edges;
  const std::set<V> &shuffled_operations;
  std::list<E> &required_flips;
  std::set<OrientationState *> required_flips_classes;
};

} // namespace tsndgm

#endif // TSN_DGM_COMPLETE_FLIP_H
