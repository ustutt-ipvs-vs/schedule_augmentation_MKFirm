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

  virtual void generate() = 0;

protected:
  DisjunctiveGraphModel &dgm;
};

class RandomInitialSelectionHeuristic : public InitialSelectionHeuristic {
public:
  RandomInitialSelectionHeuristic(DisjunctiveGraphModel &dgm)
      : InitialSelectionHeuristic(dgm), gen(rd()) {}

  void generate();

protected:
  std::random_device rd;
  std::mt19937 gen;
};

class MakespanInitialSelectionHeuristic : public InitialSelectionHeuristic {
public:
  MakespanInitialSelectionHeuristic(DisjunctiveGraphModel &dgm)
      : InitialSelectionHeuristic(dgm) {}

  void generate();
};

// Based on "A Fast Taboo Search Algorithm for the Job Shop Problem" by
// Eugeniusz Nowicki and Czeslaw Smutnicki (1996)
//
// We use a slightly simpler version, where we select a processing order that
// directly minimizes the makespan (out of all possible permutations).
// In the original version, the authors propose using the processing order that
// minimizes the cost of the longest path from src -> sink that contains the
// newly inserted operation
class INSA : public InitialSelectionHeuristic {
public:
  INSA(DisjunctiveGraphModel &dgm) : InitialSelectionHeuristic(dgm) {}

  void generate();
};

} // namespace tsndgm

#endif // TSN_DGM_HEURISTIC_INITIAL_H
