#ifndef TSN_DGM_DGM_H
#define TSN_DGM_DGM_H

#include "../network/message_stream.h"
#include "../network/topology.h"
#include "critical_path.h"
#include "shuffle_graph.h"
#include "traversal.h"
#include <boost/graph/adjacency_list.hpp>

namespace tsndgm {

class DisjunctiveGraphModel {
public:
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  shuffle_graph_t shuffle_graph;
  std::list<E> flip_log;

  /** \brief Construct a disjunctive graph from a network topology and a set of
   * message streams.
   *
   * \param network shared pointer to network topology.
   * \param streams vector of message streams.
   */
  DisjunctiveGraphModel(const std::shared_ptr<NetworkTopology> &network,
                        const std::vector<MessageStream> &streams)
      : network(network), crit_path(shuffle_graph) {
    shuffle_graph[boost::graph_bundle].src = boost::add_vertex(shuffle_graph);
    shuffle_graph[boost::graph_bundle].sink = boost::add_vertex(shuffle_graph);
    shuffle_graph[boost::graph_bundle].streams = streams;

    build();
  }

  CriticalPath::Result critical_path(CriticalPath::Objective type);

  /** \brief Perform a complete flip operation on the shuffle graph.
   *
   * \param edge edge descriptor of edge to be flipped.
   */
  inline void complete_flip(E edge) {
    complete_flip(std::list<E>(std::initializer_list<E>{edge}));
  };

  /** \brief Perform a complete flip operation on the shuffle graph.
   *
   * \param edge_list list of edge descriptors of edges to be flipped.
   */
  inline void complete_flip(const std::list<E> &edge_list) {
    std::set<boost::OrientationState *> flipped_edges;
    complete_flip(edge_list, flipped_edges);
  }

  /** \brief Perform a complete flip operation on the shuffle graph.
   *
   * \param edge_list list of edge descriptors of edges to be flipped.
   * \param flipped_edges set of pointers to OrientationState objects that
   * represent equivalence classes of already flipped edges.
   */
  void complete_flip(const std::list<E> &edge_list,
                     std::set<boost::OrientationState *> &flipped_edges);

  /** \brief Perform a lazy shuffle operation on the shuffle graph.
   *
   * \param edge edge descriptor of edge whose source and target are to be
   * shuffled.
   */
  void lazy_shuffle(E edge);

  /** \brief Perform a lazy shuffle operation on the shuffle graph.
   *
   * \param edge_list list of edge descriptors of edges whose source and target
   * are to be shuffled.
   */
  void lazy_shuffle(std::list<E> edge_list);

  /** \brief Perform a split all operation on the shuffle graph.
   *
   * This operation undoes all previous shuffle operations but keeps the
   * orientation of all edges that were not shuffled.
   */
  void split_all();

  /** \brief Commit performed flips such that a later call to
   * restore_last_commit does not undo previous changes.
   *
   * Every CompleteFlip operation logs the flipped edges, enabling an efficient
   * rollback (which is mostly useful for enumerating a selection's
   * neighborhood). Importantly, however, a LazyShuffle operation invalidates
   * this log. To rollback an LazyShuffle, use commit_all().
   */
  void commit_flips();

  /** \brief Commits the shuffle graph by copying it completely.
   *
   * This is required if rollbacks of LazyShuffle operations are desired.
   * In particular, LazyShuffle operations modify the shuffle graph extensively
   * (by merging edge equivalence classes, updating weights, and edge
   * contraction of shuffled operations). As a result, manual logging every
   * change is rather complex but likely does not increase performance
   * drastically.
   */
  void commit_all(size_t index = 1);

  void restore_flips();
  void restore_commit(size_t index = 1, bool swap = false);
  void copy_commit(size_t src_index, size_t dest_index);
  void swap_commit(size_t src_index, size_t dest_index);

  /** \brief Print the shuffle graph to stdout.
   */
  void print();

  void print_critical_path(CriticalPath::Objective type);

  E edge(V u, V v) {
    auto e = boost::edge(u, v, shuffle_graph);
    if (!e.second)
      throw std::runtime_error("edge (" + std::to_string(u) + ", " +
                               std::to_string(v) + ") does not exist");
    return e.first;
  }

private:
  std::shared_ptr<NetworkTopology> network;

  CriticalPath crit_path;
  bool valid_crit_path = false;

  std::vector<shuffle_graph_t> committed_shuffle_graphs;

  void build();
  void build_stream(MessageStreamHandle handle);
  void resize_properties();

  void remove_fifo_edges(V u, V v);
};

} // namespace tsndgm

#endif // TSN_DGM_DGM_H
