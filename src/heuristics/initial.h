#ifndef TSN_DGM_HEURISTIC_INITIAL_H
#define TSN_DGM_HEURISTIC_INITIAL_H

#include "../dgm/dgm.h"
#include <random>

namespace tsndgm {

class InitialSelectionHeuristic {
public:
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  InitialSelectionHeuristic(DisjunctiveGraphModel &dgm) : dgm(dgm) {}

  virtual void generate(CriticalPath::Objective type) = 0;

protected:
  DisjunctiveGraphModel &dgm;
};

class RandomInitial : public InitialSelectionHeuristic {
public:
  RandomInitial(DisjunctiveGraphModel &dgm)
      : InitialSelectionHeuristic(dgm), gen(rd()) {}

  void generate(CriticalPath::Objective type);

protected:
  std::random_device rd;
  std::mt19937 gen;
};

// Based on "A Fast Taboo Search Algorithm for the Job Shop Problem" by
// Eugeniusz Nowicki and Czeslaw Smutnicki (1996)
//
// We use a slightly simpler version, where we select a processing order that
// directly minimizes the makespan (out of all possible permutations).
// In the original version, the authors propose using the processing order that
// minimizes the cost of the longest path from src -> sink that contains the
// newly inserted operation
class INSA : public RandomInitial {
public:
  INSA(DisjunctiveGraphModel &dgm) : RandomInitial(dgm) {}

  void generate(CriticalPath::Objective type);
};

} // namespace tsndgm

#endif // TSN_DGM_HEURISTIC_INITIAL_H
