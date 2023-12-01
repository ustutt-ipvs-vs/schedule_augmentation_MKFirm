#ifndef TSN_DGM_INTENSIFICATION_TCC
#define TSN_DGM_INTENSIFICATION_TCC

#include "intensification.h"
#include "neighborhood.h"

namespace tsndgm {

template <class TerminationCriterion, class SelectionNeighborhood>
NextSelection
StrictAdmissionIntensification<TerminationCriterion, SelectionNeighborhood>::
    compute_next_selection(CriticalPath::Objective type, bool commit_best) {
  CriticalPath::Result res = this->dgm.critical_path(type);
  Neighborhood neighborhood = selection_neighborhood.compute(res);
  ExtendedNextSelection next_selection;

  // store current DGM, which is used to undo flips
  this->dgm.commit_flips();

  // compute next neighbor
  for (E e : neighborhood.flip_candidates) {
    this->dgm.complete_flip(e);
    auto res = this->dgm.critical_path(type);

    if (res.objective <
        std::min(this->best_selection.objective, next_selection.objective)) {
      // res.objective is better than the objective of the best selection
      // found in this intensification phase (aspiration criterion)
      next_selection = {e, flip, res.objective, this->config.tabu_tenure};
    } else {
      // Check tabu list otherwise.
      // In case every neighbor violates the tabu list, we ensure that
      // we return the neighbor which violates the oldest entry.
      size_t violation = compute_first_violation(e);
      if (violation > next_selection.violation ||
          (violation == next_selection.violation &&
           res.objective < next_selection.objective)) {
        next_selection = {e, flip, res.objective, violation};
      }
    }

    this->dgm.restore_flips();
  }

  tabu_list.resize(next_selection.violation, tabu_list.back());
  update_tabu_list(next_selection.e);

  if (next_selection.objective < this->best_selection.objective) {
    if (commit_best)
      this->dgm.commit_all(this->best_selection.commit_index);
    this->best_selection.objective = next_selection.objective;
  }

  return next_selection;
}

} // namespace tsndgm

#endif // TSN_DGM_INTENSIFICATION_TCC
