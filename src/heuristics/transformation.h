#ifndef TSN_DGM_HEURISTIC_TRANSFORMATION_H
#define TSN_DGM_HEURISTIC_TRANSFORMATION_H

#include "../dgm/dgm.h"
#include <random>

namespace tsndgm {

class Transformation {
public:
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  Transformation(DisjunctiveGraphModel &dgm, CriticalPath::Objective type)
      : dgm(dgm), type(type), gen(rd()) {}

  bool compute_best_permutation(V v);
  virtual void transform(int k) = 0;

protected:
  DisjunctiveGraphModel &dgm;
  CriticalPath::Objective type;
  CriticalPath::Result int_phase_result;
  std::random_device rd;
  std::mt19937 gen;

  virtual bool accept(const std::vector<V> &processing_order,
                      const std::pair<Delay, int> &min_d,
                      const CriticalPath::Result &div_result, int v_pos,
                      int cur_pos) {
    return false;
  };

  void update_machine_successors();
  void print_before_and_after(std::map<Edge, std::vector<V>> &before);
};

class NoTransformation : public Transformation {
public:
  NoTransformation(DisjunctiveGraphModel &dgm, CriticalPath::Objective type)
      : Transformation(dgm, type) {}

  void transform(int k){};
};

class RandomOperationTransformation : public Transformation {
public:
  RandomOperationTransformation(DisjunctiveGraphModel &dgm,
                                CriticalPath::Objective type)
      : Transformation(dgm, type), d(0, 1) {}
  virtual void transform(int k);

protected:
  std::uniform_real_distribution<> d;

  bool accept(const std::vector<V> &processing_order,
              const std::pair<Delay, int> &min_d,
              const CriticalPath::Result &div_result, int v_pos, int cur_pos);
};

class RandomCriticalPathTransformation : public RandomOperationTransformation {
public:
  RandomCriticalPathTransformation(DisjunctiveGraphModel &dgm,
                                   CriticalPath::Objective type)
      : RandomOperationTransformation(dgm, type) {}
  virtual void transform(int k);
};

class SlackTransformation : public RandomOperationTransformation {
public:
  SlackTransformation(DisjunctiveGraphModel &dgm, CriticalPath::Objective type)
      : RandomOperationTransformation(dgm, type) {}
  virtual void transform(int k);

protected:
  auto sample_operation();
};

} // namespace tsndgm
#endif // TSN_DGM_HEURISTIC_TRANSFORMATION_H
