#ifndef TSN_DGM_DGM_H
#define TSN_DGM_DGM_H

#include "../network/message_stream.h"
#include "../network/route.h"
#include "shuffle_graph.h"
#include "traversal.h"
#include <boost/graph/adjacency_list.hpp>

namespace tsndgm {

class DisjunctiveGraphModel {
public:
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  shuffle_graph_t shuffle_graph;

  /** \brief Construct a disjunctive graph from a network topology and a set of
   * message streams.
   *
   * \param network shared pointer to network topology.
   * \param streams vector of message streams.
   */
  DisjunctiveGraphModel(const std::shared_ptr<NetworkTopology> &network,
                        const std::vector<MessageStream> &streams)
      : prop(boost::get_property(shuffle_graph, boost::graph_bundle)),
        network(network) {
    prop.src = boost::add_vertex(shuffle_graph);
    prop.sink = boost::add_vertex(shuffle_graph);
    prop.streams = streams;

    build();
  }

  inline void complete_flip(E edge) {
    complete_flip(std::list<E>(std::initializer_list<E>{edge}));
  };
  void complete_flip(std::list<E> edge_list);

  void print();

private:
  ShuffleGraphProperty
      &prop; //!< reference to graph properties of shuffle_graph
  shuffle_graph_t
      initial_shuffle_graph; //!< backup copy of initial_shuffle_graph for
                             //!< split_all operation

  std::shared_ptr<NetworkTopology> network;

  void build();
  void build_stream(MessageStreamHandle handle);

  void copy_graph();
};

} // namespace tsndgm

#endif // TSN_DGM_DGM_H
