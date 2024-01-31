#ifndef TSN_DGM_SELECTION_STORAGE_H
#define TSN_DGM_SELECTION_STORAGE_H

#include "../dgm/dgm.h"
#include "selection.h"
#include <random>

namespace tsndgm {

class SelectionStorage {
public:
  typedef std::vector<std::pair<Edge, size_t>> DifferingMachines;
  typedef std::vector<std::set<DisjunctiveGraphModel::V>>
      GuidingMachineProcessingOrder;

  std::vector<EncodedSelection> encoded_best_selections;

  SelectionStorage(DisjunctiveGraphModel &dgm, size_t max_stored_solutions)
      : dgm(dgm), max_stored_solutions(max_stored_solutions), gen(rd()) {}

  void update_candidates(BestSelection &res);
  void update_candidates(EncodedSelection &&selection);
  bool wants(Delay objective);
  EncodedSelection &sample();

  size_t get_processing_index(EncodedSelection &selection, Edge edge,
                              MessageStreamHandle ms);

  inline size_t size() { return encoded_best_selections.size(); }
  inline size_t capacity() { return max_stored_solutions; }

private:
  DisjunctiveGraphModel &dgm;
  size_t max_stored_solutions;

  std::random_device rd;
  std::mt19937 gen;
};

} // namespace tsndgm

#endif // TSN_DGM_SELECTION_STORAGE_H
