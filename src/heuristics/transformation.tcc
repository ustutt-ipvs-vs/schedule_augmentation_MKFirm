#ifndef TSN_DGM_HEURISTIC_TRANSFORMATION_TCC
#define TSN_DGM_HEURISTIC_TRANSFORMATION_TCC

#include "transformation.h"

namespace tsndgm {

template <class T> double Transformation<T>::p_barrier(E uv) {
  assert((dgm.shuffle_graph[uv].edge_type != conjunctive));
  if (dgm.shuffle_graph[uv].edge_type == fifo)
    uv = dgm.fifo_to_disjunctive_edge(uv);

  V u = source(uv, dgm.shuffle_graph), v = target(uv, dgm.shuffle_graph);
  Edge edge = dgm.shuffle_graph[u].edge;
  MessageStreamHandle ms_u = dgm.shuffle_graph[u].ms_handle.front(),
                      ms_v = dgm.shuffle_graph[v].ms_handle.front();

  Delay uv_objective = 0, vu_objective = 0;
  for (auto &selection : storage.encoded_best_selections) {
    if (uv_objective > 0 && vu_objective > 0)
      break;
    size_t u_pos = storage.get_processing_index(selection, edge, ms_u);
    size_t v_pos = storage.get_processing_index(selection, edge, ms_v);
    if (u_pos < v_pos && uv_objective == 0)
      uv_objective = selection.objective;
    else if (v_pos < u_pos && vu_objective == 0)
      vu_objective = selection.objective;
  }
  assert((uv_objective > 0 || vu_objective > 0));

  Delay base =
      storage
          .encoded_best_selections[storage.encoded_best_selections.size() - 1]
          .objective -
      storage.encoded_best_selections[0].objective;
  if (base == 0)
    return 0.5;

  // no best_selection contains the edge uv; hence, always flip
  if (temperature > temperature_schedule.c && uv_objective == 0)
    return 0;
  // no best_selection contains the edge vu; hence, never flip
  else if (temperature > temperature_schedule.c && vu_objective == 0)
    return 1;

  return p /
         (p + (1 - p) *
                  exp(-temperature *
                      static_cast<double>(vu_objective - uv_objective) / base));
}

template <class T>
bool Transformation<T>::transform(Neighborhood &neighborhood) {
  shuffle_graph_t &shuffle_graph = this->dgm.shuffle_graph;
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  auto &candidates = neighborhood.flip_candidates;

  for (size_t i = 0; i < candidates.size(); i++) {
    std::uniform_int_distribution<> dc(i, candidates.size() - 1);
    size_t j = dc(gen);
    std::swap(candidates[i], candidates[j]);

    while (candidates[i].size() > 0) {
      if (!already_flipped(candidates[i]) &&
          d(gen) > p_barrier(candidates[i].back())) {
        try {
          dgm.complete_flip(candidates[i]);
        } catch (FlipGraphException &e) {
          break;
        }
        flipped_edges.push_front(candidates[i]);
        return true;
      }
      candidates[i].pop_back();
    }
  }

  return false;
}

template <class T> int RandomCriticalPathTransformation<T>::transform(int k) {
  shuffle_graph_t &shuffle_graph = this->dgm.shuffle_graph;
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
  this->flipped_edges.clear();

  for (int i = 0; i < k; i++) {
    SelectionCriticalBlockNeighborhood selection_neighborhood(this->dgm);
    Neighborhood neighborhood =
        selection_neighborhood.compute(this->dgm.critical_path(this->type));
    if (!Transformation<T>::transform(neighborhood))
      return i;
  }
  return k;
}

template <class T> bool Transformation<T>::already_flipped(std::list<E> edges) {
  for (E e : edges) {
    for (auto &entry : flipped_edges) {
      if (std::find(entry.begin(), entry.end(), e) != entry.end())
        return true;
    }
  }
  return false;
}

} // namespace tsndgm

#endif
