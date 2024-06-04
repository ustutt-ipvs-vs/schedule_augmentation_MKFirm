#ifndef TSN_DGM_transmission_graph_H
#define TSN_DGM_transmission_graph_H

#include "../network/message_stream.h"
#include "../network/route.h"
#include <boost/graph/adjacency_list.hpp>
#include <iomanip>

namespace tsndgm {
typedef std::pair<Edge, MessageStreamHandle> Operation;

struct TransmissionGraphVertexProperty;
struct TransmissionGraphEdgeProperty;
struct TransmissionGraphProperty;

// TODO: consider replacing OutEdgeList with boost::setS
// ~> faster lookup speed with boost::edge(), but slower iteration over graph...
typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS,
                              TransmissionGraphVertexProperty,
                              TransmissionGraphEdgeProperty,
                              TransmissionGraphProperty>
    transmission_graph_t;

struct TransmissionGraphProperty {
  typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

  V src;
  V sink;
  std::vector<MessageStream> streams;
  std::map<Edge, std::set<MessageStreamHandle>> edge_to_streams;
  std::map<Operation, V> operation_to_vertex;

  std::vector<Delay> crit_cost;
  std::vector<Delay> slack;
  std::vector<V> crit_pred;

  Delay hyperperiod;
};

struct TransmissionGraphVertexProperty {
  typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

  Edge edge;
  std::list<MessageStreamHandle> ms_handle;
  std::list<const TreeRouteHop *> hop;

  TransmissionGraphVertexProperty &
  operator=(const TransmissionGraphVertexProperty &other) = default;
};

enum TransmissionGraphEdgeType { conjunctive, disjunctive, fifo };

struct TransmissionGraphEdgeProperty {
  typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

  Delay weight;
  TransmissionGraphEdgeType edge_type;

  TransmissionGraphEdgeProperty &
  operator=(const TransmissionGraphEdgeProperty &other) = default;
};

static void
print(const transmission_graph_t &transmission_graph,
      const NetworkTopology &network,
      boost::graph_traits<transmission_graph_t>::vertex_descriptor v) {
  const TransmissionGraphProperty &prop =
      transmission_graph[boost::graph_bundle];

  if (v == prop.src) {
    std::cout << "src";
  } else if (v == prop.sink) {
    std::cout << "sink";
  } else {
    std::cout << "([" << network[transmission_graph[v].edge.first] << ", "
              << network[transmission_graph[v].edge.second] << "], {";
    if (transmission_graph[v].ms_handle.size() == 0)
      std::cout << "})";
    for (auto handle : transmission_graph[v].ms_handle)
      std::cout << prop.streams[handle].name
                << (handle == transmission_graph[v].ms_handle.back() ? "})"
                                                                     : ", ");
  }
}

static void
print(const transmission_graph_t &transmission_graph,
      const NetworkTopology &network,
      boost::graph_traits<transmission_graph_t>::edge_descriptor e) {
  int n = log10(boost::num_vertices(transmission_graph)) + 1;
  std::cout << "(" << std::setfill('0') << std::setw(n)
            << source(e, transmission_graph) << ", " << std::setfill('0')
            << std::setw(n) << target(e, transmission_graph) << "): ";
  print(transmission_graph, network, source(e, transmission_graph));
  std::cout << " -> ";
  print(transmission_graph, network, target(e, transmission_graph));
  std::cout << ": " << transmission_graph[e].weight << ", "
            << transmission_graph[e].edge_type;
}

static void print(const transmission_graph_t &transmission_graph,
                  const NetworkTopology &network) {
  std::cout << "Operations:" << std::endl;
  for (auto v :
       boost::make_iterator_range(boost::vertices(transmission_graph))) {
    print(transmission_graph, network, v);
    std::cout << std::endl;
  }
  std::cout << "Conjunctive Edges:" << std::endl;
  for (auto e : boost::make_iterator_range(boost::edges(transmission_graph))) {
    if (transmission_graph[e].edge_type == conjunctive) {
      print(transmission_graph, network, e);
      std::cout << std::endl;
    }
  }
  std::cout << "Disjunctive Edges:" << std::endl;
  for (auto e : boost::make_iterator_range(boost::edges(transmission_graph))) {
    if (transmission_graph[e].edge_type == disjunctive) {
      print(transmission_graph, network, e);
      std::cout << std::endl;
    }
  }
  std::cout << "FIFO Edges:" << std::endl;
  for (auto e : boost::make_iterator_range(boost::edges(transmission_graph))) {
    if (transmission_graph[e].edge_type == fifo) {
      print(transmission_graph, network, e);
      std::cout << std::endl;
    }
  }
}
} // namespace tsndgm

#endif // TSN_DGM_transmission_graph_H
