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

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS,
                              ShuffleGraphVertexProperty,
                              ShuffleGraphEdgeProperty, ShuffleGraphProperty>
    shuffle_graph_t;

struct ShuffleGraphVertexProperty {
  Edge edge;
  std::list<MessageStreamHandle> ms_handle;
  std::list<const TreeRouteHop *> hop;
  std::list<boost::graph_traits<shuffle_graph_t>::vertex_descriptor> root;

  ShuffleGraphVertexProperty &
  operator=(const ShuffleGraphVertexProperty &other) = default;
};

struct OrientationStatePair {
  boost::OrientationState state;
  OrientationStatePair *reversed_state;
};

typedef std::pair<std::shared_ptr<OrientationStatePair>,
                  std::shared_ptr<OrientationStatePair>>
    PtrOrientationStatePair;

static PtrOrientationStatePair create_pair() {
  std::shared_ptr<OrientationStatePair> state =
      std::make_shared<OrientationStatePair>(boost::allowed);
  std::shared_ptr<OrientationStatePair> reversed_state =
      std::make_shared<OrientationStatePair>(boost::blocked);
  state->reversed_state = reversed_state.get();
  reversed_state->reversed_state = state.get();

  return {state, reversed_state};
}

const std::shared_ptr<OrientationStatePair> CONJUNCTIVE_STATE =
    std::make_shared<OrientationStatePair>(boost::allowed);

enum ShuffleGraphEdgeType { conjunctive, disjunctive, fifo };

struct ShuffleGraphEdgeProperty {
  // weight has to be updated on merge
  Delay weight;
  std::shared_ptr<OrientationStatePair> state_pair;
  ShuffleGraphEdgeType edge_type;

  const boost::OrientationState &state() const { return state_pair->state; }

  ShuffleGraphEdgeProperty &
  operator=(const ShuffleGraphEdgeProperty &other) = default;
};

struct ShuffleGraphProperty {
  boost::graph_traits<shuffle_graph_t>::vertex_descriptor src;
  boost::graph_traits<shuffle_graph_t>::vertex_descriptor sink;
  std::vector<MessageStream> streams;
  std::map<Edge, std::set<MessageStreamHandle>> edge_to_streams;
  std::map<Operation, boost::graph_traits<shuffle_graph_t>::vertex_descriptor>
      operation_to_vertex;

  ShuffleGraphProperty &operator=(const ShuffleGraphProperty &other) = default;
};

static void print(const shuffle_graph_t &shuffle_graph,
                  const ShuffleGraphProperty &prop,
                  boost::graph_traits<shuffle_graph_t>::vertex_descriptor v) {
  if (v == prop.src) {
    std::cout << "src";
  } else if (v == prop.sink) {
    std::cout << "sink";
  } else {
    std::cout << "([" << shuffle_graph[v].edge.first << ", "
              << shuffle_graph[v].edge.second << "], {";
    for (auto handle : shuffle_graph[v].ms_handle)
      std::cout << handle
                << (handle == shuffle_graph[v].ms_handle.back() ? "})" : ", ");
  }
}

static void print(const shuffle_graph_t &shuffle_graph,
                  const ShuffleGraphProperty &prop,
                  boost::graph_traits<shuffle_graph_t>::edge_descriptor e) {
  print(shuffle_graph, prop, source(e, shuffle_graph));
  std::cout << " -> ";
  print(shuffle_graph, prop, target(e, shuffle_graph));
  std::cout << ": " << shuffle_graph[e].weight;
}

static void print(const shuffle_graph_t &shuffle_graph,
                  const ShuffleGraphProperty &prop) {
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
