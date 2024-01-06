#ifndef TSN_DGM_HEURISTIC_INITIAL_H
#define TSN_DGM_HEURISTIC_INITIAL_H

#include "../dgm/dgm.h"
#include "transformation.h"
#include <random>

namespace tsndgm {

class InitialSelectionHeuristic {
public:
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  InitialSelectionHeuristic(DisjunctiveGraphModel &dgm,
                            CriticalPath::Objective type)
      : dgm(dgm), type(type) {}

  virtual void generate() = 0;

protected:
  DisjunctiveGraphModel &dgm;
  CriticalPath::Objective type;
};

class NoInitial : public InitialSelectionHeuristic {
public:
  NoInitial(DisjunctiveGraphModel &dgm, CriticalPath::Objective type)
      : InitialSelectionHeuristic(dgm, type) {}

  void generate() {}
};

class RandomInitial : public InitialSelectionHeuristic {
public:
  RandomInitial(DisjunctiveGraphModel &dgm, CriticalPath::Objective type)
      : InitialSelectionHeuristic(dgm, type), gen(rd()) {}

  void generate();

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
class InsertionInitialHeuristic : public RandomInitial {
public:
  InsertionInitialHeuristic(DisjunctiveGraphModel &dgm,
                            CriticalPath::Objective type)
      : RandomInitial(dgm, type) {}

  void generate();
};

class RandomTransformHeuristic : public RandomInitial, public Transformation {
public:
  RandomTransformHeuristic(DisjunctiveGraphModel &dgm,
                           CriticalPath::Objective type)
      : RandomInitial(dgm, type), Transformation(dgm, type) {}

  void generate();
  void transform(int k);
};

} // namespace tsndgm

#endif // TSN_DGM_HEURISTIC_INITIAL_H
