#ifndef TSN_DGM_DGM_H
#define TSN_DGM_DGM_H

#include "../network/message_stream.h"
#include "../network/topology.h"
#include "complete_flip.h"
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
    std::set<V> shuffled_operations = {};
    complete_flip(flipped_edges, shuffled_operations, e);
  }

  inline void complete_shuffle(std::list<E> &edges) {
    for (E e : edges)
      complete_shuffle(e);
  }
  /** Notes:
   *  - recursively shuffles until no more FlipGraphException occurs
   *  - invalidates flip_log
   */
  inline void complete_shuffle(E e) {
    internal_commit_all(shuffle_fallback);
    std::set<OrientationState *> flipped_edges;
    std::set<V> shuffled_operations;
    try {
      complete_shuffle(e, flipped_edges, shuffled_operations);
    } catch (UnfixableCycleException &e) {
      internal_restore_commit(shuffle_fallback, true);
      throw;
    }
  }
  void split_all();

  inline void commit_flips() { flip_log.clear(); }
  inline void restore_flips() { restore_flips(flip_log.size()); }
  void restore_flips(size_t n);
  inline void commit_all(size_t index = 0) {
    internal_commit_all(index + EXTERNAL_COMMIT_OFFSET);
  }
  void restore_commit(size_t index = 0, bool swap = false) {
    internal_restore_commit(index + EXTERNAL_COMMIT_OFFSET, swap);
  }

  void encode(std::vector<unsigned int> &buf);
  void decode(std::vector<unsigned int> &buf, size_t last_commit = 0);

  inline void apply_processing_order(
      const std::map<Edge, std::vector<V>> &processing_order) {
    for (auto &[edge, operations] : processing_order)
      apply_machine_processing_order(operations);
  };
  void apply_machine_processing_order(const std::vector<V> &operations);

  inline void print() { tsndgm::print(shuffle_graph, *network); }
  inline void print(V v) { tsndgm::print(shuffle_graph, *network, v); }
  inline void print(E e) { tsndgm::print(shuffle_graph, *network, e); }
  inline void print_critical_path(CriticalPath::Objective type) {
    crit_path.print(critical_path(type), *network);
  }

  inline E edge(V u, V v) {
    auto e = boost::edge(u, v, shuffle_graph);
    if (!e.second)
      throw std::runtime_error("edge (" + std::to_string(u) + ", " +
                               std::to_string(v) + ") does not exist");
    return e.first;
  }

  inline E edge(E uv) {
    V u = source(uv, shuffle_graph), v = target(uv, shuffle_graph);
    return edge(u, v);
  }

  inline E rev_edge(E uv) {
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

  /** CommitIndices is used to name the internal usage of commit indices.
   * initial: fallback: used in the split_all operation to undo all shuffles
   * shuffle_fallback: used if shuffling results in an UnfixableCycleExcpetion
   * EXTERNAL_OFFSET: so that external commits do not interfere
   */
  enum CommitIndices { initial, shuffle_fallback, EXTERNAL_COMMIT_OFFSET };
  std::vector<shuffle_graph_t> committed_shuffle_graphs;

  void build();
  void build_stream(MessageStreamHandle handle);
  void resize_properties();

  void internal_commit_all(size_t index);
  void internal_restore_commit(size_t index, bool swap);

  // If !uv.has_value(), complete_flip eliminates cycles with
  // at least one edge in flipped_edges
  void complete_flip(std::set<OrientationState *> &flipped_edges,
                     const std::set<V> &shuffled_operations,
                     std::optional<E> uv);
  void complete_shuffle(E e, std::set<OrientationState *> &flipped_edges,
                        std::set<V> &shuffled_operations);

  void remove_fifo_edges(V u, V v);
  void renew_descriptors();

  void update_machine_successors(std::map<V, V> updates);
};

} // namespace tsndgm

#endif // TSN_DGM_DGM_H
