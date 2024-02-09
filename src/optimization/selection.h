#ifndef TSN_DGM_SELECTIONS_H
#define TSN_DGM_SELECTIONS_H

#include "../dgm/dgm.h"

namespace tsndgm {

enum DGMOperation { flip, shuffle };

struct BestSelection {
  size_t *commit_index;
  Delay objective;
  bool committed;

  BestSelection() {}

  BestSelection(size_t *commit_index,
                Delay objective = std::numeric_limits<Delay>::max())
      : commit_index(commit_index), objective(objective), committed(false) {}

  bool operator<(const BestSelection &best_selection) {
    return objective < best_selection.objective;
  }

  bool operator<=(const BestSelection &best_selection) {
    return objective <= best_selection.objective;
  }
};

struct EncodedSelection {
  Delay objective;
  std::vector<unsigned int> buf;
  OffsetMap offset_map;

  EncodedSelection(DisjunctiveGraphModel &dgm, BestSelection &best_selection)
      : objective(best_selection.objective) {
    dgm.encode(buf, *best_selection.commit_index, offset_map);
  }

  EncodedSelection(DisjunctiveGraphModel &dgm, CriticalPath::Objective type) {
    objective = dgm.critical_path(type).objective;
    dgm.encode(buf, offset_map);
  }

  EncodedSelection(Delay objective, std::vector<unsigned int> &&buf)
      : objective(objective), buf(std::move(buf)) {
    for (int i = 0; i < this->buf.size(); i++) {
      if (this->buf[i] == MACHINE_SEPARATOR && i + 2 < this->buf.size()) {
        Edge edge(this->buf[i + 1], this->buf[i + 2]);
        offset_map[edge] = i;
      }
    }
  }
};

struct NextSelection {
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  std::list<E> edges;
  DGMOperation operation;
  Delay objective;

  NextSelection() : objective(std::numeric_limits<Delay>::max()) {}
  NextSelection(std::list<E> edges, DGMOperation operation, Delay objective)
      : edges(edges), operation(operation), objective(objective) {}

  bool operator<(const NextSelection &other) {
    return objective < other.objective;
  }

  bool operator<(const BestSelection &best_selection) {
    return objective < best_selection.objective;
  }

  bool operator<=(const BestSelection &best_selection) {
    return objective <= best_selection.objective;
  }
};

} // namespace tsndgm

#endif // TSN_DGM_SELECTIONS_H
