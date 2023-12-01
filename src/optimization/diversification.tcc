#ifndef TSN_DGM_DIVERSIFICATION_TCC
#define TSN_DGM_DIVERSIFICATION_TCC

#include "diversification.h"
#include "neighborhood.h"
#include <random>

namespace tsndgm {

template <class SelectionNeighborhood>
void FrequencyCountDiversification<SelectionNeighborhood>::update_history(
    const NextSelection &next_selection) {
  frequency_count[next_selection.e]++;
}

template <class SelectionNeighborhood>
void FrequencyCountDiversification<SelectionNeighborhood>::run(
    CriticalPath::Objective type) {
  size_t iteration;
  CriticalPath::Result res = this->dgm.critical_path(type);

  for (iteration = 0;
       !this->termination_criterion.satisfied(iteration, res.objective);
       iteration++) {
    Neighborhood neighborhood = this->selection_neighborhood.compute(res);
    std::vector<E> edges;
    std::vector<double> weights;
    for (E e : neighborhood.flip_candidates) {
      edges.push_back(e);
      weights.push_back(1.0 / static_cast<double>(frequency_count[e]));
    }
    std::discrete_distribution<int> d(weights.begin(), weights.end());
    E e = edges[d(gen)];
    frequency_count[e]++;
    this->dgm.complete_flip(e);
    res = this->dgm.critical_path(type);
  }

  std::cout << " -> diversify: " << res.objective << " after " << iteration
            << " iterations" << std::endl;
}

} // namespace tsndgm

#endif // TSN_DGM_DIVERSIFICATION_TCC
