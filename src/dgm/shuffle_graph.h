#ifndef TSN_DGM_SHUFFLE_GRAPH_H
#define TSN_DGM_SHUFFLE_GRAPH_H

#include "../network/message_stream.h"
#include "../network/route.h"
#include "traversal.h"
#include <boost/graph/adjacency_list.hpp>

namespace tsndgm {

typedef unsigned int MessageStreamHandle;
typedef std::pair<Edge, MessageStreamHandle> Operation;

struct ShuffleGraphVertexProperty;
struct ShuffleGraphEdgeProperty;
struct ShuffleGraphProperty;

// TODO: consider replacing OutEdgeList with boost::setS
// ~> faster lookup speed with boost::edge(), but slower iteration over graph...
typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS,
                              ShuffleGraphVertexProperty,
                              ShuffleGraphEdgeProperty, ShuffleGraphProperty>
    shuffle_graph_t;

struct ShuffleGraphProperty {
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;

  V src;
  V sink;
  std::vector<MessageStream> streams;
  std::map<Edge, std::set<MessageStreamHandle>> edge_to_streams;
  std::map<Operation, V> operation_to_vertex;

  std::vector<Delay> crit_cost;
  std::vector<V> crit_pred;
  std::vector<V> cycle_pred;
};

struct ShuffleGraphVertexProperty {
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;

  Edge edge;
  std::list<MessageStreamHandle> ms_handle;
  std::list<const TreeRouteHop *> hop;
  std::list<V> root;

  ShuffleGraphVertexProperty &
  operator=(const ShuffleGraphVertexProperty &other) = default;

  void shuffle(ShuffleGraphVertexProperty &other) {
    ms_handle.splice(ms_handle.end(), other.ms_handle);
    hop.splice(hop.end(), other.hop);
    root.splice(root.end(), other.root);
  }
};

struct PtrOrientationStatePair {
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  std::shared_ptr<boost::OrientationState> state;
  std::shared_ptr<boost::OrientationState> reversed_state;

  std::set<std::pair<V, V>> equivalence_class;

  std::vector<std::set<std::pair<V, V>>> committed_equivalence_classes;
  std::vector<boost::OrientationState> committed_states;

  void add_edge(E e, shuffle_graph_t &shuffle_graph) {
    equivalence_class.insert(
        {source(e, shuffle_graph), target(e, shuffle_graph)});
  }

  void commit(size_t index) {
    for (size_t i = committed_equivalence_classes.size(); i <= index; i++) {
      committed_equivalence_classes.push_back({});
      committed_states.push_back(*state);
    }

    committed_equivalence_classes[index] = equivalence_class;
    committed_states[index] = *state;
  }

  void restore_commit(size_t index, bool swap) {
    if (swap)
      std::swap(equivalence_class, committed_equivalence_classes[index]);
    else
      equivalence_class = committed_equivalence_classes[index];

    if (*state != committed_states[index]) {
      std::swap(*state, *reversed_state);
      if (swap)
        committed_states[index] = *reversed_state;
    }
  }

  void copy_commit(size_t src_index, size_t dest_index) {
    committed_equivalence_classes[dest_index] =
        committed_equivalence_classes[src_index];
    committed_states[dest_index] = committed_states[src_index];
  }

  void swap_commit(size_t src_index, size_t dest_index) {
    std::swap(committed_equivalence_classes[src_index],
              committed_equivalence_classes[dest_index]);
    std::swap(committed_states[dest_index], committed_states[src_index]);
  }

  PtrOrientationStatePair(
      std::shared_ptr<boost::OrientationState> state,
      std::shared_ptr<boost::OrientationState> reversed_state)
      : state(state), reversed_state(reversed_state) {}
};

static PtrOrientationStatePair create_pair() {
  std::shared_ptr<boost::OrientationState> state =
      std::make_shared<boost::OrientationState>(boost::allowed);
  std::shared_ptr<boost::OrientationState> reversed_state =
      std::make_shared<boost::OrientationState>(boost::blocked);

  return PtrOrientationStatePair(state, reversed_state);
}

const std::shared_ptr<PtrOrientationStatePair> CONJUNCTIVE_STATE =
    std::make_shared<PtrOrientationStatePair>(create_pair());

enum ShuffleGraphEdgeType { conjunctive, disjunctive, fifo };

struct ShuffleGraphEdgeProperty {
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  Delay weight;
  std::shared_ptr<PtrOrientationStatePair> state_pair;
  ShuffleGraphEdgeType edge_type;

  ShuffleGraphEdgeProperty &
  operator=(const ShuffleGraphEdgeProperty &other) = default;

  const boost::OrientationState &state() const { return *state_pair->state; }
  const boost::OrientationState &reversed_state() const {
    return *state_pair->reversed_state;
  }

  void consistent_flip() {
    std::swap(*state_pair->state, *state_pair->reversed_state);
  }

  bool relates_to(const ShuffleGraphEdgeProperty &other) {
    return state_pair->state.get() == other.state_pair->state.get();
  }

  void merge_equivalence_classes(E other, shuffle_graph_t &shuffle_graph,
                                 ShuffleGraphProperty &prop) {
    for (auto e : state_pair->equivalence_class) {
      auto e_new = boost::edge(e.first, e.second, shuffle_graph);
      if (e_new.second) {
        shuffle_graph[e_new.first].state_pair = shuffle_graph[other].state_pair;
        shuffle_graph[other].state_pair->equivalence_class.insert(e);
      }
    }
  }
};

static void print(const shuffle_graph_t &shuffle_graph,
                  const ShuffleGraphProperty &prop,
                  boost::graph_traits<shuffle_graph_t>::vertex_descriptor v) {
  if (v == prop.src) {
    std::cout << "src";
  } else if (v == prop.sink) {
    std::cout << "sink";
  } else {
    std::cout << " ([" << shuffle_graph[v].edge.first << ", "
              << shuffle_graph[v].edge.second << "], {";
    for (auto handle : shuffle_graph[v].ms_handle)
      std::cout << handle
                << (handle == shuffle_graph[v].ms_handle.back() ? "})" : ", ");
  }
}

static void print(const shuffle_graph_t &shuffle_graph,
                  const ShuffleGraphProperty &prop,
                  boost::graph_traits<shuffle_graph_t>::edge_descriptor e) {
  std::cout << e << ": ";
  print(shuffle_graph, prop, source(e, shuffle_graph));
  std::cout << " -> ";
  print(shuffle_graph, prop, target(e, shuffle_graph));
  std::cout << ": " << shuffle_graph[e].weight;
}

static void print(const shuffle_graph_t &shuffle_graph,
                  const ShuffleGraphProperty &prop) {

  std::cout << "Operations:" << std::endl;
  for (auto vd : boost::make_iterator_range(boost::vertices(shuffle_graph))) {
    std::cout << vd << ": ";
    print(shuffle_graph, prop, vd);
    std::cout << ": " << shuffle_graph[boost::graph_bundle].crit_cost[vd]
              << " (" << shuffle_graph[boost::graph_bundle].crit_pred[vd] << ")"
              << std::endl;
  }

  std::cout << "Conjunctive Edges:" << std::endl;
  for (auto ed : boost::make_iterator_range(boost::edges(shuffle_graph))) {
    if (shuffle_graph[ed].edge_type == conjunctive) {
      print(shuffle_graph, prop, ed);
      std::cout << std::endl;
    }
  }

  std::cout << "Disjunctive Edges:" << std::endl;
  for (auto ed : boost::make_iterator_range(boost::edges(shuffle_graph))) {
    if (shuffle_graph[ed].edge_type == disjunctive) {
      if (shuffle_graph[ed].state() == boost::allowed) {
        print(shuffle_graph, prop, ed);
        std::cout << std::endl;
      }
    }
  }

  std::cout << "FIFO Edges:" << std::endl;
  for (auto ed : boost::make_iterator_range(boost::edges(shuffle_graph))) {
    if (shuffle_graph[ed].edge_type == fifo) {
      if (shuffle_graph[ed].state() == boost::allowed) {
        print(shuffle_graph, prop, ed);
        std::cout << std::endl;
      }
    }
  }
}

} // namespace tsndgm

#endif // TSN_DGM_SHUFFLE_GRAPH_H
