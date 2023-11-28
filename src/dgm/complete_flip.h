#ifndef TSN_DGM_COMPLETE_FLIP_H
#define TSN_DGM_COMPLETE_FLIP_H

#include "critical_path.h"

namespace tsndgm {

class complete_flip_visitor : public longest_path_visitor {
public:
  complete_flip_visitor(
      shuffle_graph_t &shuffle_graph,
      std::set<boost::OrientationState *> &flipped_edges,
      std::set<boost::OrientationState *> &required_flips_classes,
      std::list<E> &required_flips)
      : longest_path_visitor(shuffle_graph), flipped_edges(flipped_edges),
        required_flips_classes(required_flips_classes),
        required_flips(required_flips) {}

  void tree_edge(E e, const shuffle_graph_t &shuffle_graph) const {
    prop.cycle_pred[source(e, shuffle_graph)] = target(e, shuffle_graph);
  }

  void back_edge(E uv, const shuffle_graph_t &shuffle_graph) const {
    // uv may be flipped by previously called back_edge
    if (required_flips_classes.contains(
            shuffle_graph[uv].state_pair->state.get()))
      return;

    V u = source(uv, shuffle_graph), v = target(uv, shuffle_graph), w = v;
    bool flipped_uv =
        flipped_edges.contains(shuffle_graph[uv].state_pair->state.get());
    std::pair<E, bool> prev = std::make_pair(uv, flipped_uv);

    std::optional<E> candidate = {};
    while (w != u) {
      E e = boost::edge(w, prop.cycle_pred[w], shuffle_graph).first;
      w = prop.cycle_pred[w];

      // conjunctive edges cannot be flipped, continue
      if (shuffle_graph[e].edge_type == conjunctive)
        continue;

      // check if cycle is already eliminated by previously called back_edge
      if (required_flips_classes.contains(
              shuffle_graph[e].state_pair->state.get()))
        return;

      if (!flipped_edges.contains(shuffle_graph[e].state_pair->state.get()))
        prev = std::make_pair(e, false);
      else if (!prev.second)
        candidate = prev.first;
    }
    if (flipped_uv && !prev.second)
      candidate = prev.first;

    if (candidate.has_value()) {
      E e = candidate.value();
      required_flips.push_back(e);
      required_flips_classes.insert(shuffle_graph[e].state_pair->state.get());
    } else if (prev.second) {
      // todo: can this case ever occur?
      throw std::runtime_error(
          "Found cycle where every disjunctive edge was already flipped");
    } else {
      throw std::runtime_error("Shuffle graph was not acyclic before flip");
    }
  }

  std::set<boost::OrientationState *> &flipped_edges;
  std::set<boost::OrientationState *> &required_flips_classes;
  std::list<E> &required_flips;
};

} // namespace tsndgm

#endif // TSN_DGM_COMPLETE_FLIP_H
