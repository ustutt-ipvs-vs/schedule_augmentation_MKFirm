#ifndef TSN_DGM_INTENSIFICATION_TCC
#define TSN_DGM_INTENSIFICATION_TCC

#include "intensification.h"
#include "neighborhood.h"

namespace tsndgm {

template <class TerminationCriterion, class SelectionNeighborhood>
NextSelection
StrictAdmissionIntensification<TerminationCriterion, SelectionNeighborhood>::
    compute_next_selection(CriticalPath::Objective type) {
  CriticalPath::Result res = this->dgm.critical_path(type);
  ExtendedNextSelection next_selection;

  // store current DGM, which is used to undo flips
  this->dgm.commit_flips();
  int extension_level = 0;
  do {
    Neighborhood neighborhood =
        selection_neighborhood.extend(res, extension_level);

    // compute next neighbor
    for (auto &edges : neighborhood.flip_candidates) {
      try {
        this->dgm.complete_flip(edges);
      } catch (FlipGraphException &e) {
        this->dgm.restore_flips();
        if (this->config.recursive_shuffle) {
          auto res = this->dgm.critical_path(type);
          next_selection = {
              {e.required_shuffle}, shuffle, res.objective, this->config.maxt};
          return next_selection;
        } else {
          continue;
        }
      }
      auto res = this->dgm.critical_path(type);
      size_t violation = compute_first_violation({edges, flip, res.objective});

      if (res.objective <
          std::min(this->best_selection.objective, next_selection.objective)) {
        // res.objective is better than the objective of the best selection
        // found in this intensification phase (aspiration criterion)
        next_selection = {edges, flip, res.objective, this->config.maxt};
      } else {
        // Check tabu list otherwise.
        // In case every neighbor violates the tabu list, we ensure that
        // we return the neighbor which violates the oldest entry.
        if (violation > next_selection.violation ||
            (violation == next_selection.violation &&
             res.objective < next_selection.objective)) {
          next_selection = {edges, flip, res.objective, violation};
        }
      }

      this->dgm.restore_flips();
    }

    if (next_selection.objective < this->best_selection.objective)
      this->best_selection.objective = next_selection.objective;
    extension_level++;
  } while (extension_level <= SelectionNeighborhood::max_extension &&
           next_selection.violation < this->tabu_list.size());

  if (this->tabu_list.size() != 0)
    this->tabu_list.resize(next_selection.violation, this->tabu_list.back());
  update_tabu_list({next_selection.edges, this->best_selection.objective});

  return next_selection;
}

template <class TerminationCriterion, class SelectionNeighborhood>
void StrictAdmissionIntensification<TerminationCriterion,
                                    SelectionNeighborhood>::reset_phase() {
  this->termination_criterion = TerminationCriterion(this->config.maxit);
  this->best_selection = BestSelection(0);
  this->clear_tabu_list();
}

template <class TerminationCriterion, class SelectionNeighborhood>
void StrictAdmissionIntensification<
    TerminationCriterion, SelectionNeighborhood>::update_tabu_list(TabuListEntry
                                                                       entry) {
  this->tabu_list.push_front(entry);

  if (this->tabu_list.size() > this->config.maxt)
    this->tabu_list.pop_back();
}

} // namespace tsndgm

#endif // TSN_DGM_INTENSIFICATION_TCC
