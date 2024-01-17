#ifndef TSN_DGM_RELINKING_PHASE_H
#define TSN_DGM_RELINKING_PHASE_H

#include "../dgm/dgm.h"
#include "intensification.h"
#include "selection.h"
#include <random>

namespace tsndgm {
struct RelinkingConfig {
  IntensificationConfig local_search_config;
  int min_separation = 2;
  double p = 0.5;
  size_t min_stored_solutions = 8;
  size_t max_stored_solutions = 30;
  size_t best_commit_index = 3;
  size_t backup_commit_index = 4;
};

class Relinking {
public:
  typedef std::vector<std::pair<Edge, size_t>> DifferingMachines;
  typedef std::vector<std::set<DisjunctiveGraphModel::V>>
      GuidingMachineProcessingOrder;

  std::vector<EncodedSelection> encoded_best_selections;

  Relinking() : gen(rd()) {}

  inline void initialize(DisjunctiveGraphModel *dgm) { this->dgm = dgm; }

  void update_candidates(BestSelection &res);

  inline bool ready(RelinkingConfig &config) {
    return encoded_best_selections.size() >= config.min_stored_solutions;
  }

  inline bool differs(BestSelection &res, int guiding_index) {
    return compute_differing_machines(res, guiding_index).size() > 0;
  }

  std::pair<int, int> sample(RelinkingConfig &config);

  DifferingMachines compute_differing_machines(int initial_index,
                                               int guiding_index);
  DifferingMachines compute_differing_machines(BestSelection &res,
                                               int guiding_index);
  DifferingMachines compute_differing_machines(EncodedSelection &initial,
                                               EncodedSelection &guiding);

  GuidingMachineProcessingOrder
  get_guiding_processing_order(const EncodedSelection &guiding_selection,
                               Edge edge, int offset);

  bool step_towards(EncodedSelection &guiding_selection,
                    DifferingMachines &differing_machines, int steps);

private:
  DisjunctiveGraphModel *dgm;

  std::random_device rd;
  std::mt19937 gen;

  bool step_towards(const EncodedSelection &guiding_selection,
                    DifferingMachines &differing_machines);

  inline int skip_until(const EncodedSelection &s, int i, unsigned int v) {
    while (s.buf[i] != v)
      i++;
    return i;
  }
};

} // namespace tsndgm

#include "relinking_phase.tcc"

#endif // TSN_DGM_RELINKING_PHASE_H
