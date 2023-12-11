#ifndef TSN_DGM_TERMINATION_H
#define TSN_DGM_TERMINATION_H

#include "../dgm/critical_path.h"

namespace tsndgm {

struct TerminationCriterion {
  TerminationCriterion(size_t max_iterations)
      : max_iterations(max_iterations) {}

  virtual bool satisfied(size_t iteration, Delay objective) = 0;
  virtual ~TerminationCriterion() = default;

  size_t max_iterations;
};

struct DefaultTerminationCriterion : public TerminationCriterion {
  DefaultTerminationCriterion(size_t max_iterations)
      : TerminationCriterion(max_iterations) {}

  bool satisfied(size_t iteration, Delay objective) {
    return iteration >= max_iterations || objective == 0;
  }
};

struct DifferentialTerminationCriterion : public TerminationCriterion {
  DifferentialTerminationCriterion(size_t max_iterations)
      : TerminationCriterion(max_iterations) {}

  bool satisfied(size_t iteration, Delay objective) {
    if (objective < best_solution.objective) {
      best_solution = {objective, iteration};
    }
    return iteration - best_solution.iteration >= max_iterations;
  }

  struct BestSolution {
    Delay objective;
    size_t iteration;
  };
  BestSolution best_solution = {std::numeric_limits<Delay>::max(), 0};
};

// Extension 1: Termination criterion via timer (e.g. run tabu search for 2
// minutes)

// Extension 2: max_iterations after finding last best solution

} // namespace tsndgm

#endif // TSN_DGM_TERMINATION_H
