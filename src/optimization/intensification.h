#ifndef TSN_DGM_INTENSIFICATION_H
#define TSN_DGM_INTENSIFICATION_H

#include "selection.h"

namespace tsndgm {

struct IntensificationConfig {
  size_t maxt;  //!< max size of tabu list
  size_t maxit; //!< max iterations
  size_t commit_index;
  bool recursive_shuffle;

  IntensificationConfig(size_t maxt, size_t maxit)
      : maxt(maxt), maxit(maxit), commit_index(0), recursive_shuffle(false) {}

private:
  IntensificationConfig(size_t maxt, size_t maxit, size_t commit_index,
                        bool recursive_shuffle = false)
      : maxt(maxt), maxit(maxit), commit_index(commit_index),
        recursive_shuffle(recursive_shuffle) {}

  friend class ExhaustiveSearchConfig;
  friend class RelinkingConfig;
};

struct ExhaustiveSearchConfig : public IntensificationConfig {
  ExhaustiveSearchConfig(size_t maxt, size_t maxit)
      : IntensificationConfig(maxt, maxit, 1, false) {}
};

template <class TerminationCriterion, class SelectionNeighborhood>
class Intensification {
public:
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;
  typedef TerminationCriterion ITerminationCriterion;
  typedef SelectionNeighborhood ISelectionNeighborhood;

  Intensification(DisjunctiveGraphModel &dgm, IntensificationConfig &config,
                  Delay termination_bound = 0)
      : dgm(dgm), config(config), best_selection(0),
        termination_criterion(config.maxit, termination_bound){};

  virtual NextSelection
  compute_next_selection(CriticalPath::Objective type) = 0;

  template <typename TabuListEntry> void update_tabu_list(TabuListEntry entry);

  bool completed(size_t iteration, Delay objective) {
    return termination_criterion.satisfied(iteration, objective);
  };

  virtual void reset_phase() = 0;
  virtual void clear_tabu_list() = 0;

  virtual ~Intensification() = default;

  BestSelection best_selection;

protected:
  DisjunctiveGraphModel &dgm;
  IntensificationConfig &config;
  TerminationCriterion termination_criterion;
};

template <class TerminationCriterion, class SelectionNeighborhood>
class StrictAdmissionIntensification
    : public Intensification<TerminationCriterion, SelectionNeighborhood> {
public:
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  struct TabuListEntry {
    std::list<E> edges;
    Delay objective;
  };
  typedef std::list<TabuListEntry> TabuList;

  struct ExtendedNextSelection : public NextSelection {
    size_t violation;

    ExtendedNextSelection() : NextSelection(), violation(0) {}

    ExtendedNextSelection(std::list<E> edges, DGMOperation operation,
                          Delay objective, size_t violation)
        : NextSelection(edges, operation, objective), violation(violation) {}
  };

  StrictAdmissionIntensification(DisjunctiveGraphModel &dgm,
                                 IntensificationConfig &config,
                                 Delay termination_bound = 0)
      : Intensification<TerminationCriterion, SelectionNeighborhood>(
            dgm, config, termination_bound),
        selection_neighborhood(dgm) {}

  NextSelection compute_next_selection(CriticalPath::Objective type);

  virtual void update_tabu_list(TabuListEntry entry);

  void reset_phase();
  inline void clear_tabu_list() { tabu_list.clear(); }

  TabuList tabu_list;

protected:
  SelectionNeighborhood selection_neighborhood;

  virtual inline size_t compute_first_violation(NextSelection n) {
    return std::distance(
        tabu_list.cbegin(),
        std::find_if(tabu_list.cbegin(), tabu_list.cend(), [&](auto &entry) {
          return std::all_of(entry.edges.begin(), entry.edges.end(), [&](E e) {
            return this->dgm.shuffle_graph[e].state() != blocked;
          });
        }));
  }
};

template <class TerminationCriterion, class SelectionNeighborhood>
class TestStrictIntensification
    : public StrictAdmissionIntensification<TerminationCriterion,
                                            SelectionNeighborhood> {
public:
  TestStrictIntensification(DisjunctiveGraphModel &dgm,
                            IntensificationConfig &config,
                            Delay termination_bound = 0)
      : StrictAdmissionIntensification<TerminationCriterion,
                                       SelectionNeighborhood>(
            dgm, config, termination_bound) {}

protected:
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  inline size_t compute_first_violation(NextSelection n) {
    return std::distance(
        this->tabu_list.cbegin(),
        std::find_if(
            this->tabu_list.cbegin(), this->tabu_list.cend(), [&](auto &entry) {
              return std::all_of(
                  entry.edges.begin(), entry.edges.end(), [&](E e) {
                    return this->dgm.shuffle_graph[e].state() != blocked &&
                           entry.objective <= n.objective;
                  });
            }));
  }
};

template <class TerminationCriterion, class SelectionNeighborhood>
class RelaxedAdmissionIntensification
    : public StrictAdmissionIntensification<TerminationCriterion,
                                            SelectionNeighborhood> {
public:
  RelaxedAdmissionIntensification(DisjunctiveGraphModel &dgm,
                                  IntensificationConfig &config,
                                  Delay termination_bound = 0)
      : StrictAdmissionIntensification<TerminationCriterion,
                                       SelectionNeighborhood>(
            dgm, config, termination_bound) {}

  void update_tabu_list(
      StrictAdmissionIntensification<
          TerminationCriterion, SelectionNeighborhood>::TabuListEntry entry) {
    std::list<E> edges;
    for (E &e : entry.edges) {
      edges.push_back(this->dgm.edge(target(e, this->dgm.shuffle_graph),
                                     source(e, this->dgm.shuffle_graph)));
    }

    StrictAdmissionIntensification<
        TerminationCriterion,
        SelectionNeighborhood>::update_tabu_list({edges, entry.objective});
  }

protected:
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  inline size_t compute_first_violation(NextSelection n) {
    return std::distance(
        this->tabu_list.cbegin(),
        std::find_if(this->tabu_list.cbegin(), this->tabu_list.cend(),
                     [&](auto &entry) {
                       return std::any_of(
                           entry.edges.begin(), entry.edges.end(), [&](E e) {
                             return std::find(n.edges.begin(), n.edges.end(),
                                              e) != n.edges.end();
                           });
                     }));
  }
};

template <class TerminationCriterion, class SelectionNeighborhood>
class TestRelaxedIntensification
    : public RelaxedAdmissionIntensification<TerminationCriterion,
                                             SelectionNeighborhood> {
public:
  TestRelaxedIntensification(DisjunctiveGraphModel &dgm,
                             IntensificationConfig &config,
                             Delay termination_bound = 0)
      : RelaxedAdmissionIntensification<TerminationCriterion,
                                        SelectionNeighborhood>(
            dgm, config, termination_bound) {}

protected:
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  inline size_t compute_first_violation(NextSelection n) {
    return std::distance(
        this->tabu_list.cbegin(),
        std::find_if(this->tabu_list.cbegin(), this->tabu_list.cend(),
                     [&](auto &entry) {
                       return std::any_of(
                           entry.edges.begin(), entry.edges.end(), [&](E e) {
                             return std::find(n.edges.begin(), n.edges.end(),
                                              e) != n.edges.end() &&
                                    entry.objective <= n.objective;
                           });
                     }));
  }
};

} // namespace tsndgm

#include "intensification.tcc"

#endif // TSN_DGM_INTENSIFICATION_H
