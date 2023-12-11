#ifndef TSN_DGM_DIVERSIFICATION_TCC
#define TSN_DGM_DIVERSIFICATION_TCC

#include "diversification.h"
#include "neighborhood.h"
#include <random>

namespace tsndgm {

template <class SelectionNeighborhood>
void FrequencyCountDiversification<SelectionNeighborhood>::update_history(
    const NextSelection &next_selection) {
  for (E e : this->dgm.flip_log) {
    std::pair<V, V> pair = {source(e, this->dgm.shuffle_graph),
                            target(e, this->dgm.shuffle_graph)};
    if (!frequency_count.contains(pair))
      frequency_count[pair] = 1;
    else
      frequency_count[pair]++;
  }
}

template <class SelectionNeighborhood>
NextSelection
FrequencyCountDiversification<SelectionNeighborhood>::compute_next_selection(
    CriticalPath::Objective type) {
  CriticalPath::Result res = this->dgm.critical_path(type);
  Neighborhood neighborhood = this->selection_neighborhood.compute(res);

  // compute random edge on critical path (weighted by their frequency count)
  std::vector<size_t> weights;
  std::pair<V, V> pair;

  size_t max_freq = 0;
  for (auto &edges : neighborhood.flip_candidates) {
    for (E e : edges) {
      pair = {source(e, this->dgm.shuffle_graph),
              target(e, this->dgm.shuffle_graph)};
      max_freq = std::max(max_freq, frequency_count[pair]);
    }
  }

  for (auto &edges : neighborhood.flip_candidates) {
    weights.push_back(0);
    for (E e : edges) {
      pair = {source(e, this->dgm.shuffle_graph),
              target(e, this->dgm.shuffle_graph)};
      weights.back() =
          std::max(weights.back(), max_freq - frequency_count[pair]);
    }
  }

  std::discrete_distribution<int> d(weights.begin(), weights.end());

  auto &edges = neighborhood.flip_candidates[d(gen)];
  for (E e : edges) {
    pair = {source(e, this->dgm.shuffle_graph),
            target(e, this->dgm.shuffle_graph)};
    frequency_count[pair]++;
  }

  // no need to recompute res.objective
  return {edges, flip, res.objective};
}

} // namespace tsndgm

#endif // TSN_DGM_DIVERSIFICATION_TCC
