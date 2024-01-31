#ifndef TSN_DGM_TERMINATION_H
#define TSN_DGM_TERMINATION_H

#include "../dgm/critical_path.h"
#include <chrono>

namespace tsndgm {

struct TerminationConfig {
  size_t maxit;
  Delay bound = 0;
};

struct TerminationCriterion {
  TerminationCriterion(TerminationConfig tconfig)
      : max_iterations(tconfig.maxit), bound(tconfig.bound) {}

  TerminationCriterion(size_t max_iterations, Delay bound = 0)
      : max_iterations(max_iterations), bound(bound) {}

  virtual bool satisfied(size_t iteration, Delay objective) = 0;
  virtual std::pair<Delay, Delay> progress(size_t iteration,
                                           Delay objective) = 0;
  virtual ~TerminationCriterion() = default;

  size_t max_iterations;
  Delay bound;
};

struct DefaultTerminationCriterion : public TerminationCriterion {
  DefaultTerminationCriterion(size_t max_iterations, Delay bound = 0)
      : TerminationCriterion(max_iterations, bound) {}
  DefaultTerminationCriterion(TerminationConfig tconfig)
      : TerminationCriterion(tconfig) {}

  bool satisfied(size_t iteration, Delay objective) {
    return iteration >= max_iterations || objective <= bound;
  }

  std::pair<Delay, Delay> progress(size_t iteration, Delay objective) {
    return {iteration, max_iterations};
  }
};

struct DifferentialTerminationCriterion : public TerminationCriterion {
  DifferentialTerminationCriterion(size_t max_iterations, Delay bound = 0)
      : TerminationCriterion(max_iterations, bound) {}
  DifferentialTerminationCriterion(TerminationConfig tconfig)
      : TerminationCriterion(tconfig) {}

  bool satisfied(size_t iteration, Delay objective) {
    if (objective < best_solution.objective) {
      best_solution = {objective, iteration};
    }
    return iteration - best_solution.iteration >= max_iterations ||
           objective <= bound;
  }

  std::pair<Delay, Delay> progress(size_t iteration, Delay objective) {
    return {iteration - best_solution.iteration, max_iterations};
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
  TimeoutTerminationCriterion(TerminationConfig tconfig)
      : TerminationCriterion(tconfig),
        timeout(static_cast<std::chrono::seconds>(tconfig.maxit)) {
    start = std::chrono::high_resolution_clock::now();
  }

  bool satisfied(size_t iteration, Delay objective) {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::seconds>(now - start);
    return duration >= timeout || objective <= bound;
  }

  std::pair<Delay, Delay> progress(size_t iteration, Delay objective) {
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = duration_cast<std::chrono::milliseconds>(now - start);
    return {duration.count(), 1000 * timeout.count()};
  }

protected:
  std::chrono::high_resolution_clock::time_point start;
  std::chrono::seconds timeout;
};

} // namespace tsndgm

#endif // TSN_DGM_TERMINATION_H
