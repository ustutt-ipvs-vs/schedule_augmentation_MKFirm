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

class complete_flip_visitor : public update_machine_successors_visitor {
public:
  complete_flip_visitor(shuffle_graph_t &shuffle_graph,
                        std::set<OrientationState *> &flipped_edges,
                        std::list<E> &required_flips,
                        std::map<V, V> &updated_machine_successors)
      : update_machine_successors_visitor(shuffle_graph,
                                          updated_machine_successors),
        flipped_edges(flipped_edges), required_flips(required_flips) {}

  void tree_edge(E e, const shuffle_graph_t &shuffle_graph) const {
    prop.cycle_pred[source(e, shuffle_graph)] = target(e, shuffle_graph);
  }

  bool back_edge(E uv, const shuffle_graph_t &shuffle_graph) {
    V u = source(uv, shuffle_graph), v = target(uv, shuffle_graph), w = v;

    V old_pred = prop.cycle_pred[u];
    prop.cycle_pred[u] = v;

    std::optional<E> last_disjunctive_edge = {};
    std::optional<E> breaking_edge = {};

    do {
      E e = boost::edge(w, prop.cycle_pred[w], shuffle_graph).first;
      w = prop.cycle_pred[w];

      // conjunctive edges cannot be flipped, continue
      if (shuffle_graph[e].edge_type == conjunctive) {
        continue;
      }
      if (required_flips_classes.contains(
              shuffle_graph[e].state_pair->state.get())) {
        prop.cycle_pred[u] = old_pred;
        return false;
      }

      if (!flipped_edges.contains(
              shuffle_graph[e].state_pair->reversed_state.get())) {
        last_disjunctive_edge = e;
      } else {
        if (last_disjunctive_edge.has_value()) {
          V u1 = source(*last_disjunctive_edge, shuffle_graph),
            v1 = target(*last_disjunctive_edge, shuffle_graph);
          V w1 = prop.cycle_pred[v1];
          do {
            auto wv = boost::edge(w1, v1, shuffle_graph);
            if (wv.second && shuffle_graph[wv.first].state() == allowed) {
              bool new_flip =
                  required_flips_classes
                      .insert(shuffle_graph[wv.first].state_pair->state.get())
                      .second;
              if (new_flip)
                required_flips.push_back(wv.first);
            }
            w1 = prop.cycle_pred[w1];
          } while (w1 != v1);

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
    } while (w != v || breaking_edge.has_value());

    throw std::runtime_error("shuffle graph was acyclic before complete flip");
  }

  std::set<OrientationState *> &flipped_edges;
  std::list<E> &required_flips;
  std::set<OrientationState *> required_flips_classes;
};

} // namespace tsndgm

#endif // TSN_DGM_COMPLETE_FLIP_H
