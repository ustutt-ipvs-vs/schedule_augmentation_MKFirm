#ifndef TSN_DGM_SELECTIONS_H
#define TSN_DGM_SELECTIONS_H

#include "../dgm/dgm.h"
#include "neighborhood.h"

namespace tsndgm {

enum DGMOperation { flip, shuffle };

struct EncodedSelection {
  Delay objective;
  std::vector<unsigned int> buf;
  OffsetMap offset_map;
  std::optional<Neighborhood> neighborhood;
  int extension_level = 0;

  EncodedSelection() : objective(std::numeric_limits<Delay>::max()) {}

  EncodedSelection(DisjunctiveGraphModel &dgm, CriticalPath::Objective type) {
    objective = dgm.critical_path(type).objective;
    dgm.encode(buf, offset_map);
  }

  EncodedSelection(const EncodedSelection &other)
      : objective(other.objective), buf(other.buf) {
    for (int i = 0; i < this->buf.size(); i++) {
      if (this->buf[i] == MACHINE_SEPARATOR && i + 2 < this->buf.size()) {
        Edge edge(this->buf[i + 1], this->buf[i + 2]);
        offset_map[edge] = i;
      }
    }
  }
};

struct NextSelection {
  typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

  std::list<E> edges;
  DGMOperation operation;
  Delay objective;

  NextSelection() : objective(std::numeric_limits<Delay>::max()) {}
  NextSelection(std::list<E> edges, DGMOperation operation, Delay objective)
      : edges(edges), operation(operation), objective(objective) {}

  bool operator<(const NextSelection &other) {
    return objective < other.objective;
  }
};

} // namespace tsndgm

#endif // TSN_DGM_SELECTIONS_H
