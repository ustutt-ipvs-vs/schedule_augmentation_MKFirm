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

  void set_capacity(size_t max_stored_solutions);

  void update_candidates(BestSelection &res);
  void update_candidates(EncodedSelection &&selection,
                         bool direct_update = true);
  void delete_candidate(EncodedSelection *res);

  EncodedSelection &sample(double temperature);

  size_t get_processing_index(EncodedSelection &selection, Edge edge,
                              MessageStreamHandle ms);

  inline size_t size() { return encoded_best_selections.size(); }
  inline size_t capacity() { return max_stored_solutions; }

  void renew_storage_objectives(CriticalPath::Objective type);

private:
  DisjunctiveGraphModel *dgm;
  size_t max_stored_solutions;

  std::vector<EncodedSelection> candidates;
  std::mutex candidate_mutex;

  std::random_device rd;
  std::mt19937 gen;
};

} // namespace tsndgm

#endif // TSN_DGM_SELECTION_STORAGE_H
