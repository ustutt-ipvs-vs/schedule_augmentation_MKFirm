#ifndef TSN_DGM_INTENSIFICATION_H
#define TSN_DGM_INTENSIFICATION_H

#include "selection.h"

namespace tsndgm {

struct IntensificationConfig {
  size_t tabu_tenure;  //!< size of tabu list
  size_t commit_index; //!< for best selection of current phase
  size_t max_iterations;
};

template <class TerminationCriterion, class SelectionNeighborhood>
class Intensification {
public:
  Intensification(DisjunctiveGraphModel &dgm, IntensificationConfig &config)
      : dgm(dgm), config(config), best_selection(config.commit_index),
        termination_criterion(config.max_iterations){};

  virtual NextSelection compute_next_selection(CriticalPath::Objective type,
                                               bool commit_best = true) = 0;

  bool completed(size_t iteration, Delay objective) {
    return termination_criterion.satisfied(iteration, objective);
  };

  virtual ~Intensification() = default;

  BestSelection best_selection;

protected:
  DisjunctiveGraphModel &dgm;
  IntensificationConfig &config;
  TerminationCriterion termination_criterion;

  virtual void clear_tabu_list() = 0;
};

template <class TerminationCriterion, class SelectionNeighborhood>
class StrictAdmissionIntensification
    : public Intensification<TerminationCriterion, SelectionNeighborhood> {
public:
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  struct TabuListEntry {
    E e;
    boost::OrientationState state;
  };
  typedef std::list<TabuListEntry> TabuList;

  struct ExtendedNextSelection : public NextSelection {
    size_t violation;

    ExtendedNextSelection() : NextSelection(), violation(0) {}

    ExtendedNextSelection(E e, DGMOperation operation, Delay objective,
                          size_t violation)
        : NextSelection(e, operation, objective), violation(violation) {}
  };

  StrictAdmissionIntensification(DisjunctiveGraphModel &dgm,
                                 IntensificationConfig &config)
      : Intensification<TerminationCriterion, SelectionNeighborhood>(dgm,
                                                                     config),
        selection_neighborhood(dgm) {}

  NextSelection compute_next_selection(CriticalPath::Objective type,
                                       bool commit_best = true);

  inline void clear_tabu_list() { tabu_list.clear(); }

protected:
  TabuList tabu_list;
  SelectionNeighborhood selection_neighborhood;

  inline void update_tabu_list(E e) {
    tabu_list.push_front({e, this->dgm.shuffle_graph[e].reversed_state()});
    if (tabu_list.size() > this->config.tabu_tenure)
      tabu_list.pop_back();
  }

  virtual inline size_t compute_first_violation(E e) {
    return std::distance(
        tabu_list.cbegin(),
        std::find_if(tabu_list.cbegin(), tabu_list.cend(), [&](auto &entry) {
          return this->dgm.shuffle_graph[entry.e].state() != entry.state;
        }));
  }
};

template <class TerminationCriterion, class SelectionNeighborhood>
class RelaxedAdmissionIntensification
    : public StrictAdmissionIntensification<TerminationCriterion,
                                            SelectionNeighborhood> {
public:
  RelaxedAdmissionIntensification(DisjunctiveGraphModel &dgm,
                                  IntensificationConfig &config)
      : StrictAdmissionIntensification<TerminationCriterion,
                                       SelectionNeighborhood>(dgm, config) {}

protected:
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  inline size_t compute_first_violation(E e) {
    return std::distance(
        this->tabu_list.cbegin(),
        std::find_if(this->tabu_list.cbegin(), this->tabu_list.cend(),
                     [&](auto &entry) { return e != entry.e; }));
  }
};

} // namespace tsndgm

#include "intensification.tcc"

#endif // TSN_DGM_INTENSIFICATION_H
