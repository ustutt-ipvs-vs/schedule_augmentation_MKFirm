#ifndef TSN_DGM_HEURISTIC_INITIAL_H
#define TSN_DGM_HEURISTIC_INITIAL_H

#include "../dgm/dgm.h"
#include <random>

namespace tsndgm {

class InitialSelectionHeuristic {
public:
  typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

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

class EffectiveReleaseInitial : public InitialSelectionHeuristic {
public:
  EffectiveReleaseInitial(DisjunctiveGraphModel &dgm,
                          CriticalPath::Objective type)
      : InitialSelectionHeuristic(dgm, type) {}

  void generate();
};

template <class A, class B>
class CombinedInitial : public InitialSelectionHeuristic {
public:
  CombinedInitial(DisjunctiveGraphModel &dgm, CriticalPath::Objective type)
      : InitialSelectionHeuristic(dgm, type), a(dgm, type), b(dgm, type) {}

  void generate() {
    a.generate();
    b.generate();
  }

private:
  A a;
  B b;
};

} // namespace tsndgm

#endif // TSN_DGM_HEURISTIC_INITIAL_H
