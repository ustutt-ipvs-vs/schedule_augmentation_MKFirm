#ifndef TSN_DGM_SELECTIONS_H
#define TSN_DGM_SELECTIONS_H

#include "../dgm/dgm.h"

namespace tsndgm {

enum DGMOperation { flip, shuffle };

struct BestSelection {
  size_t commit_index;
  Delay objective;

  BestSelection(size_t commit_index)
      : commit_index(commit_index),
        objective(std::numeric_limits<Delay>::max()) {}

  BestSelection(size_t commit_index, Delay objective)
      : commit_index(commit_index), objective(objective) {}
};

struct NextSelection {
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  E e;
  DGMOperation operation;
  Delay objective;

  NextSelection() : objective(std::numeric_limits<Delay>::max()) {}
  NextSelection(E e, DGMOperation operation, Delay objective)
      : e(e), operation(operation), objective(objective) {}
};

} // namespace tsndgm

#endif // TSN_DGM_SELECTIONS_H
