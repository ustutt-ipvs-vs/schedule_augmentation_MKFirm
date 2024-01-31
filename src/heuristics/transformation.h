#ifndef TSN_DGM_HEURISTIC_TRANSFORMATION_H
#define TSN_DGM_HEURISTIC_TRANSFORMATION_H

#include "../dgm/dgm.h"
#include "../optimization/neighborhood.h"
#include "../optimization/selection_storage.h"
#include <random>

namespace tsndgm {

struct TemperatureSchedule {
  TemperatureSchedule(double T_0, double c, int N) : T_0(T_0), c(c), N(N) {}
  virtual double compute(int k) { return 0; };

  int N;
  double T_0, c;
};

struct ConstantTemperature : public TemperatureSchedule {
  ConstantTemperature(double T_0, double c, int N)
      : TemperatureSchedule(T_0, c, N) {}

  double compute(int k) { return 0; }
};

struct ConstantThenExponentialTemperature : public TemperatureSchedule {
  ConstantThenExponentialTemperature(double T_0, double c, int N)
      : TemperatureSchedule(T_0, c, N) {
    assert((q < T_0));
    alpha = pow((1 - q / T_0), 1 / (r * (1 - c) * N));
  }

  double compute(int k) {
    if (k <= c * N)
      return 0;

    return T_0 * (1 - pow(alpha, k - c * N));
  }

  double alpha;
  double q = 0.95, r = 0.9;
};

struct ConstantThenSlowTemperature : public TemperatureSchedule {
  ConstantThenSlowTemperature(double T_0, double c, int N)
      : TemperatureSchedule(T_0, c, N) {}

  double compute(int k) {
    if (k <= c * N)
      return 0;

    return (1 - T_0) *
           exp((log(1) - log(1 - T_0)) * ((k - c * N) / ((1 - c) * N)));
  }
};

template <class T> class Transformation {
public:
  static_assert(std::is_base_of_v<TemperatureSchedule, T>);
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  Transformation(DisjunctiveGraphModel &dgm, SelectionStorage &storage,
                 CriticalPath::Objective type, double T_0, double c, int N)
      : dgm(dgm), storage(storage), type(type), gen(rd()), d(0, 1),
        temperature_schedule(T_0, c, N) {}

  virtual int transform(int k) = 0;
  bool transform(Neighborhood &neighborhood);

  inline void update_temperature(int k) {
    temperature = temperature_schedule.compute(k);
  }

  bool already_flipped(std::list<E> edge);

  double p_barrier(E e);
  std::list<std::list<E>> flipped_edges;
  double temperature = 0;

protected:
  DisjunctiveGraphModel &dgm;
  SelectionStorage &storage;
  CriticalPath::Objective type;
  T temperature_schedule;

  std::random_device rd;
  std::mt19937 gen;
  std::uniform_real_distribution<> d;
  double p = 0.5;
};

class NoTransformation : public Transformation<TemperatureSchedule> {
public:
  NoTransformation(DisjunctiveGraphModel &dgm, SelectionStorage &storage,
                   CriticalPath::Objective type, double T_0, double c, int N)
      : Transformation<TemperatureSchedule>(dgm, storage, type, T_0, c, N) {}

  int transform(int k) { return 0; };
};

template <class T>
class RandomCriticalPathTransformation : public Transformation<T> {
public:
  RandomCriticalPathTransformation(DisjunctiveGraphModel &dgm,
                                   SelectionStorage &storage,
                                   CriticalPath::Objective type, double T_0,
                                   double c, int N)
      : Transformation<T>(dgm, storage, type, T_0, c, N) {}

  virtual int transform(int k);
};

} // namespace tsndgm

#include "transformation.tcc"

#endif // TSN_DGM_HEURISTIC_TRANSFORMATION_H
