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

  SelectionStorage(DisjunctiveGraphModel *dgm)
      : dgm(dgm), max_stored_solutions(0), gen(rd()) {}

  SelectionStorage(DisjunctiveGraphModel *dgm, const SelectionStorage &other)
      : dgm(dgm), max_stored_solutions(other.max_stored_solutions), gen(rd()),
        encoded_best_selections(other.encoded_best_selections) {}

  void set_capacity(size_t max_stored_solutions);

  void update_candidates(EncodedSelection &&selection);
  void update_candidates(EncodedSelection &selection);
  void delete_candidate(EncodedSelection *res);

  EncodedSelection &sample(double temperature);

  size_t get_processing_index(EncodedSelection &selection, Edge edge,
                              MessageStreamHandle ms);

  inline size_t size() { return encoded_best_selections.size(); }
  inline size_t capacity() { return max_stored_solutions; }

  EncodedSelection &best();

  void renew_storage_objectives(CriticalPath::Objective type);

private:
  EncodedSelection best_selection;
  DisjunctiveGraphModel *dgm;
  size_t max_stored_solutions;

  std::random_device rd;
  std::mt19937 gen;
};

} // namespace tsndgm

#endif // TSN_DGM_SELECTION_STORAGE_H
