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
  Neighborhood neighborhood = selection_neighborhood.compute(res);
  ExtendedNextSelection next_selection;

  // store current DGM, which is used to undo flips
  this->dgm.commit_flips();

  // compute next neighbor
  for (auto &edges : neighborhood.flip_candidates) {
    this->dgm.complete_flip(edges);
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

  if (tabu_list.size() != 0)
    tabu_list.resize(next_selection.violation, tabu_list.back());
  update_tabu_list({next_selection.edges, this->best_selection.objective});

  return next_selection;
}

template <class TerminationCriterion, class SelectionNeighborhood>
void StrictAdmissionIntensification<TerminationCriterion,
                                    SelectionNeighborhood>::reset_phase() {
  this->termination_criterion =
      TerminationCriterion(this->config.max_iterations);
  this->best_selection = BestSelection(this->best_selection.commit_index);
  // clearing likely steers tabu search back into the local minimum of the last
  // phase
  // clear_tabu_list();

  for (TabuListEntry &entry : tabu_list) {
    for (E &e : entry.edges) {
      V s = source(e, this->dgm.shuffle_graph);
      V t = target(e, this->dgm.shuffle_graph);
      e = this->dgm.edge(s, t);
    }
  }
}

template <class TerminationCriterion, class SelectionNeighborhood>
void StrictAdmissionIntensification<
    TerminationCriterion, SelectionNeighborhood>::update_tabu_list(TabuListEntry
                                                                       entry) {
  tabu_list.push_front(entry);

  if (tabu_list.size() > this->config.maxt)
    tabu_list.pop_back();
}

} // namespace tsndgm

#endif // TSN_DGM_INTENSIFICATION_TCC
