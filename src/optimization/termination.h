#ifndef TSN_DGM_TERMINATION_H
#define TSN_DGM_TERMINATION_H

#include "../dgm/critical_path.h"
#include <chrono>

namespace tsndgm {

struct TerminationCriterion {
  TerminationCriterion(size_t max_iterations, Delay bound = 0)
      : max_iterations(max_iterations), bound(bound) {}

  virtual bool satisfied(size_t iteration, Delay objective) = 0;
  virtual ~TerminationCriterion() = default;

  size_t max_iterations;
  Delay bound;
};

struct DefaultTerminationCriterion : public TerminationCriterion {
  DefaultTerminationCriterion(size_t max_iterations, Delay bound = 0)
      : TerminationCriterion(max_iterations, bound) {}

  bool satisfied(size_t iteration, Delay objective) {
    return iteration >= max_iterations || objective <= bound;
  }
};

struct DifferentialTerminationCriterion : public TerminationCriterion {
  DifferentialTerminationCriterion(size_t max_iterations, Delay bound = 0)
      : TerminationCriterion(max_iterations, bound) {}

  bool satisfied(size_t iteration, Delay objective) {
    if (objective < best_solution.objective) {
      best_solution = {objective, iteration};
    }
    return iteration - best_solution.iteration >= max_iterations ||
           objective <= bound;
  }

  struct BestSolution {
    Delay objective;
    size_t iteration;
  };
  BestSolution best_solution = {std::numeric_limits<Delay>::max(), 0};
};

struct TimeoutTerminationCriterion : public TerminationCriterion {
  TimeoutTerminationCriterion(size_t timeout, Delay bound = 0)
      : TerminationCriterion(timeout, bound),
        timeout(static_cast<std::chrono::seconds>(timeout)) {
    start = std::chrono::high_resolution_clock::now();
  }

  bool satisfied(size_t iteration, Delay objective) {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::seconds>(now - start);
    return duration >= timeout || objective <= bound;
  }

protected:
  std::chrono::high_resolution_clock::time_point start;
  std::chrono::seconds timeout;
};

} // namespace tsndgm

#endif // TSN_DGM_TERMINATION_H
