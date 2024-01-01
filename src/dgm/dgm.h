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

  DisjunctiveGraphModel(const DisjunctiveGraphModel &other)
      : shuffle_graph(other.shuffle_graph), flip_log(other.flip_log),
        network(other.network),
        committed_shuffle_graphs(other.committed_shuffle_graphs),
        crit_path(shuffle_graph) {}

  CriticalPath::Result critical_path(CriticalPath::Objective type);

  inline void complete_flip(std::list<E> &edges) {
    for (E e : edges)
      complete_flip(e);
  }
  inline void complete_flip(E e) {
    assert((shuffle_graph[e].state() == allowed));
    assert_synchronicity(shuffle_graph);
    std::set<OrientationState *> flipped_edges;
    complete_flip(flipped_edges, e);
  }

  inline void lazy_shuffle(std::list<E> &edges) {
    for (E e : edges)
      lazy_shuffle(e);
  }
  inline void lazy_shuffle(E e) {
    std::set<OrientationState *> flipped_edges;
    lazy_shuffle(e, flipped_edges);
  }
  void lazy_shuffle(E e, std::set<OrientationState *> &flipped_edges);
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

  // Restore all flips that are stored in flip_log
  inline void restore_flips() { restore_flips(flip_log.size()); }
  // Restore last n flips that are stored in flip_log
  // Note that flip_log is LIFO
  void restore_flips(size_t n);

  void restore_commit(size_t index = 1, bool swap = false);
  void copy_commit(size_t src_index, size_t dest_index);
  void swap_commit(size_t src_index, size_t dest_index);

  void encode(std::vector<unsigned int> &buf);
  void decode(std::vector<unsigned int> &buf, size_t last_commit = 0);

  inline void apply_processing_order(
      const std::map<Edge, std::vector<V>> &processing_order) {
    for (auto &[edge, operations] : processing_order)
      apply_machine_processing_order(operations);
  };
  void apply_machine_processing_order(const std::vector<V> &operations);

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

  E edge(E uv) {
    V u = source(uv, shuffle_graph), v = target(uv, shuffle_graph);
    return edge(u, v);
  }

  E rev_edge(E uv) {
    V u = source(uv, shuffle_graph), v = target(uv, shuffle_graph);
    if (shuffle_graph[uv].edge_type == disjunctive) {
      return edge(v, u);
    } else if (shuffle_graph[uv].edge_type == fifo) {
      // for fifo edges, vu does not exist
      for (NeighborVertex &JS : shuffle_graph[v].JS) {
        auto e = boost::edge(JS.v, u, shuffle_graph);
        if (e.second) {
          return e.first;
        }
      }
      // this should never happen
      throw std::runtime_error("shuffle graph is invalid");
    }

    throw std::runtime_error("operation not supported for edges of type: " +
                             std::to_string(shuffle_graph[uv].edge_type));
  }

private:
  std::shared_ptr<NetworkTopology> network;

  CriticalPath crit_path;
  bool valid_crit_path = false;

  std::vector<shuffle_graph_t> committed_shuffle_graphs;

  void build();
  void build_stream(MessageStreamHandle handle);
  void resize_properties();

  // If !uv.has_value(), complete_flip eliminates cycles with
  // at least one edge in flipped_edges
  void complete_flip(std::set<OrientationState *> &flipped_edges,
                     std::optional<E> uv);

  void remove_fifo_edges(V u, V v);
  void renew_descriptors();

  void update_machine_successors(std::map<V, V> updates);
};

} // namespace tsndgm

#endif // TSN_DGM_DGM_H
