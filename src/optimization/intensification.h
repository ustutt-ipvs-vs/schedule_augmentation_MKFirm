#ifndef TSN_DGM_INTENSIFICATION_H
#define TSN_DGM_INTENSIFICATION_H

#include "neighborhood.h"
#include "selection.h"

namespace tsndgm {

struct IntensificationConfig {
  size_t maxt = 10;               //!< max size of tabu list
  size_t maxit = 10;              //!< max iterations
  bool recursive_shuffle = false; //!< shuffle on FlipGraphException
};

struct TabuListEntry {
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;
  std::list<E> edges;
  Delay objective;
};

typedef std::list<TabuListEntry> TabuList;

TabuList create_tabu_list(auto &flipped_edges) {
  TabuList tabu_list;
  for (auto &edges : flipped_edges)
    tabu_list.push_back({edges, std::numeric_limits<Delay>::max()});
  return tabu_list;
}

template <class TerminationCriterion, class SelectionNeighborhood>
class Intensification {
public:
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;
  typedef TerminationCriterion ITerminationCriterion;
  typedef SelectionNeighborhood ISelectionNeighborhood;

  Intensification(DisjunctiveGraphModel &dgm, IntensificationConfig &config,
                  Delay termination_bound = 0)
      : dgm(dgm), config(config),
        best_selection(std::numeric_limits<Delay>::max()),
        termination_criterion(config.maxit, termination_bound){};

  virtual NextSelection
  compute_next_selection(CriticalPath::Objective type) = 0;

  void update_tabu_list(TabuListEntry entry);

  bool completed(size_t iteration, Delay objective) {
    return termination_criterion.satisfied(iteration, objective);
  };

  virtual void reset_phase() = 0;
  virtual void clear_tabu_list() = 0;

  virtual ~Intensification() = default;

  Delay best_selection;
  TabuList tabu_list;

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
  inline void clear_tabu_list() { this->tabu_list.clear(); }

protected:
  SelectionNeighborhood selection_neighborhood;

  virtual inline size_t compute_first_violation(NextSelection n) {
    return std::distance(
        this->tabu_list.cbegin(),
        std::find_if(
            this->tabu_list.cbegin(), this->tabu_list.cend(), [&](auto &entry) {
              return std::any_of(
                  entry.edges.begin(), entry.edges.end(), [&](E e) {
                    return this->dgm.shuffle_graph[e].state() != blocked;
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

  void update_tabu_list(TabuListEntry entry) {
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

} // namespace tsndgm

#include "intensification.tcc"

#endif // TSN_DGM_INTENSIFICATION_H
