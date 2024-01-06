#ifndef TSN_DGM_SELECTIONS_H
#define TSN_DGM_SELECTIONS_H

#include "../dgm/dgm.h"

namespace tsndgm {

enum DGMOperation { flip, shuffle };

struct BestSelection {
  size_t commit_index;
  Delay objective;
  Delay secondary_objective;
  bool committed;

  BestSelection() {}

  BestSelection(size_t commit_index,
                Delay objective = std::numeric_limits<Delay>::max(),
                Delay secondary_objective = std::numeric_limits<Delay>::max())
      : commit_index(commit_index), objective(objective),
        secondary_objective(secondary_objective), committed(false) {}

  bool operator<(const BestSelection &best_selection) {
    return objective < best_selection.objective ||
           (objective == best_selection.objective &&
            secondary_objective < best_selection.secondary_objective);
  }
};

struct NextSelection {
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  std::list<E> edges;
  DGMOperation operation;
  Delay objective;
  Delay secondary_objective;

  NextSelection()
      : objective(std::numeric_limits<Delay>::max()),
        secondary_objective(std::numeric_limits<Delay>::max()) {}
  NextSelection(std::list<E> edges, DGMOperation operation, Delay objective,
                Delay secondary_objective = std::numeric_limits<Delay>::max())
      : edges(edges), operation(operation), objective(objective),
        secondary_objective(secondary_objective) {}

  bool operator<(const BestSelection &best_selection) {
    return objective < best_selection.objective ||
           (objective == best_selection.objective &&
            secondary_objective < best_selection.secondary_objective);
  }
};

} // namespace tsndgm

#endif // TSN_DGM_SELECTIONS_H
