#ifndef TSN_DGM_SHUFFLE_GRAPH_H
#define TSN_DGM_SHUFFLE_GRAPH_H

#include "../network/message_stream.h"
#include "../network/route.h"
#include <boost/graph/adjacency_list.hpp>
#include <iomanip>

namespace tsndgm {

typedef unsigned int MessageStreamHandle;
typedef std::pair<Edge, MessageStreamHandle> Operation;
enum OrientationState { allowed, blocked };

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
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  V src;
  V sink;
  std::vector<MessageStream> streams;
  std::map<Edge, std::set<MessageStreamHandle>> edge_to_streams;
  std::map<Operation, V> operation_to_vertex;
  std::map<OrientationState *, E> equivalence_class_representative;

  std::vector<Delay> crit_cost;
  std::vector<V> crit_pred;
  std::vector<V> cycle_pred;

  bool is_zips_selection;
};

struct NeighborVertex {
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  bool operator==(const NeighborVertex &other) const {
    return v == other.v && e == other.e;
  }

  V v;
  E e;
};

struct ShuffleGraphVertexProperty {
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  Edge edge;
  std::list<MessageStreamHandle> ms_handle;
  std::list<const TreeRouteHop *> hop;
  std::list<V> root;

  std::optional<NeighborVertex> MP, MS;
  std::list<NeighborVertex> JP, JS, FS;
  std::map<V, NeighborVertex> FP;
  bool neighbors_are_valid = true;

  ShuffleGraphVertexProperty &
  operator=(const ShuffleGraphVertexProperty &other) = default;

  void clear() {
    MP = {}, MS = {};
    JP.clear(), JS.clear(), FS.clear();
    FP.clear();
  }

  void shuffle(shuffle_graph_t &shuffle_graph,
               ShuffleGraphVertexProperty &other) {
    ms_handle.splice(ms_handle.end(), other.ms_handle);
    hop.splice(hop.end(), other.hop);
    root.splice(root.end(), other.root);
  }

  void invalidate_neighbors(shuffle_graph_t &shuffle_graph) {
    ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
    neighbors_are_valid = false;
    for (auto parent : JP) {
      if (parent.v != prop.src)
        shuffle_graph[parent.v].neighbors_are_valid = false;
    }
  }
};

struct PtrOrientationStatePair {
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  std::shared_ptr<OrientationState> state;
  std::shared_ptr<OrientationState> reversed_state;

  std::set<std::pair<V, V>> equivalence_class;

  std::vector<std::set<std::pair<V, V>>> committed_equivalence_classes;
  std::vector<OrientationState> committed_states;

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

  PtrOrientationStatePair(std::shared_ptr<OrientationState> state,
                          std::shared_ptr<OrientationState> reversed_state)
      : state(state), reversed_state(reversed_state) {}
};

static PtrOrientationStatePair create_pair() {
  std::shared_ptr<OrientationState> state =
      std::make_shared<OrientationState>(allowed);
  std::shared_ptr<OrientationState> reversed_state =
      std::make_shared<OrientationState>(blocked);

  return PtrOrientationStatePair(state, reversed_state);
}

const std::shared_ptr<PtrOrientationStatePair> CONJUNCTIVE_STATE =
    std::make_shared<PtrOrientationStatePair>(create_pair());

enum ShuffleGraphEdgeType { conjunctive, disjunctive, fifo };

static void update_machine_successor(
    shuffle_graph_t &shuffle_graph,
    boost::graph_traits<shuffle_graph_t>::vertex_descriptor u,
    boost::graph_traits<shuffle_graph_t>::vertex_descriptor v) {
  auto uv = boost::edge(u, v, shuffle_graph).first;
  shuffle_graph[u].MS = {v, uv};
  shuffle_graph[v].MP = {u, uv};

  shuffle_graph[u].FS.clear();
  for (auto &v_parent : shuffle_graph[v].JP) {
    if (v_parent.v != shuffle_graph[boost::graph_bundle].src) {
      auto fuv = boost::edge(u, v_parent.v, shuffle_graph).first;
      shuffle_graph[u].FS.push_back({v_parent.v, fuv});
      shuffle_graph[v_parent.v].FP[v] = {u, fuv};
    }
  }
}

static void swap_machine_successors(
    shuffle_graph_t &shuffle_graph,
    boost::graph_traits<shuffle_graph_t>::vertex_descriptor u,
    boost::graph_traits<shuffle_graph_t>::vertex_descriptor v) {
  if (shuffle_graph[v].MS.has_value()) {
    update_machine_successor(shuffle_graph, shuffle_graph[v].MP.value().v,
                             shuffle_graph[v].MS.value().v);
  } else {
    shuffle_graph[shuffle_graph[v].MP.value().v].MS = {};
    shuffle_graph[shuffle_graph[v].MP.value().v].FS.clear();
  }
  if (shuffle_graph[u].MP.has_value()) {
    update_machine_successor(shuffle_graph, shuffle_graph[u].MP.value().v, v);
  } else {
    shuffle_graph[v].MP = {};
    for (auto &v_parent : shuffle_graph[v].JP) {
      shuffle_graph[v_parent.v].FP.erase(v);
    }
  }
  update_machine_successor(shuffle_graph, v, u);
}

struct ShuffleGraphEdgeProperty {
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  Delay weight;
  std::shared_ptr<PtrOrientationStatePair> state_pair;
  ShuffleGraphEdgeType edge_type;

  ShuffleGraphEdgeProperty &
  operator=(const ShuffleGraphEdgeProperty &other) = default;

  const OrientationState &state() const { return *state_pair->state; }
  const OrientationState &reversed_state() const {
    return *state_pair->reversed_state;
  }

  void consistent_flip(shuffle_graph_t &shuffle_graph) {
    assert((state() == allowed));

    ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];
    std::swap(*state_pair->state, *state_pair->reversed_state);

    for (auto &[u, v] : state_pair->equivalence_class) {
      auto [uv, exists] = boost::edge(u, v, shuffle_graph);

      // When complete_shuffle calls consistent_flip, some edges in the
      // equivalence class can be stale (FIFO edges). We ignore those here,
      // before deleting the edges in merge_equivalence_classes
      if (!exists || shuffle_graph[uv].edge_type != disjunctive)
        continue;

      // If v comes after u in the processing order, we invalidate the neighbors
      // of MP[u], u, MS[u], ..., MP[v], v, MS[v], since these are the
      // operations of which machine predecessors/successors change.
      V w = u;
      while (shuffle_graph[w].MS.has_value() && w != v)
        w = shuffle_graph[w].MS->v;
      if (w != v)
        continue;

      V first = shuffle_graph[u].MP.has_value() ? shuffle_graph[u].MP->v : u;
      V last = shuffle_graph[v].MS.has_value() ? shuffle_graph[v].MS->v : v;
      for (w = first; w != last; w = shuffle_graph[w].MS->v) {
        shuffle_graph[w].invalidate_neighbors(shuffle_graph);
      }
      shuffle_graph[last].invalidate_neighbors(shuffle_graph);
    }
  }

  bool relates_to(const ShuffleGraphEdgeProperty &other) const {
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

template <typename T> class NeighborVertexIterator {
public:
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  typedef std::forward_iterator_tag iterator_category;
  typedef NeighborVertex value_type;
  typedef NeighborVertex *pointer;
  typedef NeighborVertex &reference;

  NeighborVertexIterator(
      std::optional<NeighborVertex> const *MN,
      std::list<NeighborVertex> const *JN, T const *FN, size_t it = 0,
      std::optional<std::list<NeighborVertex>::const_iterator> jit = {},
      std::optional<typename T::const_iterator> fit = {})
      : MN(MN), JN(JN), FN(FN), it(it), type(restricted) {
    this->jit = jit.has_value() ? jit.value() : JN->begin();
    this->fit = fit.has_value() ? fit.value() : FN->begin();
  }

  NeighborVertexIterator() = default;
  NeighborVertexIterator &
  operator=(const NeighborVertexIterator &other) = default;

  NeighborVertexIterator &
  operator=(const shuffle_graph_t::in_edge_iterator &other) {
    BOOST_STATIC_ASSERT(
        (std::is_same<T, typename std::map<V, NeighborVertex>>::value));
    type = full_in;
    full_in_iterator = other;
    return *this;
  }

  NeighborVertexIterator &
  operator=(const shuffle_graph_t::out_edge_iterator &other) {
    BOOST_STATIC_ASSERT(
        (std::is_same<T, typename std::list<NeighborVertex>>::value));
    type = full_out;
    full_out_iterator = other;
    return *this;
  }

  const NeighborVertex &operator*() {
    if (type == restricted) {
      if (it == 0 && MN->has_value())
        return MN->value();
      else if (jit != JN->end())
        return *jit;
      else if constexpr (std::is_same<typename T::const_iterator,
                                      typename std::map<V, NeighborVertex>::
                                          const_iterator>::value)
        return (*fit).second;
      else
        return (*fit);
    } else if (type == full_in) {
      auto e = *full_in_iterator;
      ref = {e.m_source, e};
      return ref;
    } else {
      auto e = *full_out_iterator;
      ref = {e.m_target, e};
      return ref;
    }
  }

  NeighborVertexIterator &operator++() {
    if (type == restricted) {
      if (it == 0 && MN->has_value())
        it++;
      else if (jit != JN->end())
        jit++;
      else
        fit++;
    } else if (type == full_in) {
      full_in_iterator++;
    } else {
      full_out_iterator++;
    }

    return *this;
  }

  bool operator==(const NeighborVertexIterator &other) const {
    if (type != other.type)
      return false;
    if (type == restricted)
      return it == other.it && jit == other.jit && fit == other.fit;
    else if (type == full_in)
      return full_in_iterator == other.full_in_iterator;
    else
      return full_out_iterator == other.full_out_iterator;
  }

  bool operator!=(const NeighborVertexIterator &other) const {
    return !(*this == other);
  }

private:
  enum IteratorType { restricted, full_in, full_out };
  IteratorType type;

  size_t it;
  std::list<NeighborVertex>::const_iterator jit;
  T::const_iterator fit;

  std::optional<NeighborVertex> const *MN;
  std::list<NeighborVertex> const *JN;
  T const *FN;

  shuffle_graph_t::in_edge_iterator full_in_iterator;
  shuffle_graph_t::out_edge_iterator full_out_iterator;
  NeighborVertex ref;
};

template <typename T> class NeighborVertexIteratorRange {
public:
  NeighborVertexIteratorRange(
      std::optional<NeighborVertex> const *MN,
      std::list<NeighborVertex> const *JN, T const *FN, size_t it = 0,
      std::optional<std::list<NeighborVertex>::const_iterator> jit = {},
      std::optional<typename T::const_iterator> fit = {})
      : MN(MN), JN(JN), FN(FN), it(it) {
    this->jit = jit.has_value() ? jit.value() : JN->begin();
    this->fit = fit.has_value() ? fit.value() : FN->begin();
  }

  std::pair<NeighborVertexIterator<T>, NeighborVertexIterator<T>> pair() const {
    return {begin(), end()};
  }

  NeighborVertexIterator<T> begin() const {
    return NeighborVertexIterator(MN, JN, FN, it, jit, fit);
  }

  NeighborVertexIterator<T> end() const {
    return NeighborVertexIterator(MN, JN, FN, MN->has_value() ? 1 : 0,
                                  JN->end(), FN->end());
  }

private:
  size_t it;
  std::list<NeighborVertex>::const_iterator jit;
  T::const_iterator fit;

  std::optional<NeighborVertex> const *MN;
  std::list<NeighborVertex> const *JN;
  T const *FN;
};

static auto
restricted_out_edges(boost::graph_traits<shuffle_graph_t>::vertex_descriptor v,
                     const shuffle_graph_t &g) {
  return NeighborVertexIteratorRange(&g[v].MS, &g[v].JS, &g[v].FS);
}

static auto
conjunctive_out_edges(boost::graph_traits<shuffle_graph_t>::vertex_descriptor v,
                      const shuffle_graph_t &g) {
  return NeighborVertexIteratorRange(&g[v].MS, &g[v].JS, &g[v].FS,
                                     g[v].MS.has_value() ? 1 : 0,
                                     g[v].JS.begin(), g[v].FS.end());
}

static auto
fifo_out_edges(boost::graph_traits<shuffle_graph_t>::vertex_descriptor v,
               const shuffle_graph_t &g) {
  return NeighborVertexIteratorRange(&g[v].MS, &g[v].JS, &g[v].FS,
                                     g[v].MS.has_value() ? 1 : 0, g[v].JS.end(),
                                     g[v].FS.begin());
}

static auto
restricted_in_edges(boost::graph_traits<shuffle_graph_t>::vertex_descriptor v,
                    const shuffle_graph_t &g) {
  return NeighborVertexIteratorRange(&g[v].MP, &g[v].JP, &g[v].FP);
}

static auto
conjunctive_in_edges(boost::graph_traits<shuffle_graph_t>::vertex_descriptor v,
                     const shuffle_graph_t &g) {
  return NeighborVertexIteratorRange(&g[v].MP, &g[v].JP, &g[v].FP,
                                     g[v].MP.has_value() ? 1 : 0,
                                     g[v].JP.begin(), g[v].FP.end());
}

static auto
fifo_in_edges(boost::graph_traits<shuffle_graph_t>::vertex_descriptor v,
              const shuffle_graph_t &g) {
  return NeighborVertexIteratorRange(&g[v].MP, &g[v].JP, &g[v].FP,
                                     g[v].MP.has_value() ? 1 : 0, g[v].JP.end(),
                                     g[v].FP.begin());
}

static void print(const shuffle_graph_t &shuffle_graph,
                  const NetworkTopology &network,
                  boost::graph_traits<shuffle_graph_t>::vertex_descriptor v) {
  const ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];

  if (v == prop.src) {
    std::cout << "src";
  } else if (v == prop.sink) {
    std::cout << "sink";
  } else {
    std::cout << "([" << network[shuffle_graph[v].edge.first] << ", "
              << network[shuffle_graph[v].edge.second] << "], {";
    if (shuffle_graph[v].ms_handle.size() == 0)
      std::cout << "})";
    for (auto handle : shuffle_graph[v].ms_handle)
      std::cout << handle
                << (handle == shuffle_graph[v].ms_handle.back() ? "})" : ", ");
  }
}

static void print(const shuffle_graph_t &shuffle_graph,
                  const NetworkTopology &network,
                  boost::graph_traits<shuffle_graph_t>::edge_descriptor e) {
  int n = log10(boost::num_vertices(shuffle_graph)) + 1;
  std::cout << "(" << std::setfill('0') << std::setw(n)
            << source(e, shuffle_graph) << ", " << std::setfill('0')
            << std::setw(n) << target(e, shuffle_graph) << "): ";
  print(shuffle_graph, network, source(e, shuffle_graph));
  std::cout << " -> ";
  print(shuffle_graph, network, target(e, shuffle_graph));
  std::cout << ": " << shuffle_graph[e].weight;
}

static void print_processing_order(
    const shuffle_graph_t &shuffle_graph,
    boost::graph_traits<shuffle_graph_t>::vertex_descriptor v) {
  while (shuffle_graph[v].MP.has_value())
    v = shuffle_graph[v].MP.value().v;
  while (shuffle_graph[v].MS.has_value()) {
    std::cout << v << " ";
    v = shuffle_graph[v].MS.value().v;
  }
  std::cout << v << std::endl;
}

static void print(const shuffle_graph_t &shuffle_graph,
                  const NetworkTopology &network) {
  std::cout << "Operations:" << std::endl;
  for (auto vd : boost::make_iterator_range(boost::vertices(shuffle_graph))) {
    std::cout << vd << ": ";
    print(shuffle_graph, network, vd);
    std::cout << ": " << shuffle_graph[boost::graph_bundle].crit_cost[vd]
              << " (" << shuffle_graph[boost::graph_bundle].crit_pred[vd] << ")"
              << std::endl;
  }

  std::cout << "Conjunctive Edges:" << std::endl;
  for (auto vd : boost::make_iterator_range(boost::vertices(shuffle_graph))) {
    for (auto &nv : conjunctive_out_edges(vd, shuffle_graph)) {
      print(shuffle_graph, network, nv.e);
      std::cout << std::endl;
    }
  }

  std::cout << "Disjunctive Edges:" << std::endl;
  for (auto vd : boost::make_iterator_range(boost::vertices(shuffle_graph))) {
    if (shuffle_graph[vd].MS.has_value()) {
      print(shuffle_graph, network, shuffle_graph[vd].MS.value().e);
      std::cout << std::endl;
    }
  }

  std::cout << "FIFO Edges:" << std::endl;
  for (auto vd : boost::make_iterator_range(boost::vertices(shuffle_graph))) {
    for (auto &nv : fifo_out_edges(vd, shuffle_graph)) {
      print(shuffle_graph, network, nv.e);
      std::cout << std::endl;
    }
  }
}
static void assert_synchronicity(const shuffle_graph_t &shuffle_graph) {
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  typedef boost::graph_traits<shuffle_graph_t>::edge_descriptor E;

  auto &prop = shuffle_graph[boost::graph_bundle];
  std::map<Edge, std::vector<V>> operations;
  for (auto &[op, v] : prop.operation_to_vertex) {
    if (operations[op.first].size() == 0)
      operations[op.first] = {};

    if (std::find(operations[op.first].begin(), operations[op.first].end(),
                  v) != operations[op.first].end())
      continue;

    int i;
    for (i = 0; i < operations[op.first].size(); i++) {
      auto e = boost::edge(operations[op.first][i], v, shuffle_graph);
      assert((e.second));
      if (shuffle_graph[e.first].state() == blocked)
        break;
    }
    operations[op.first].insert(operations[op.first].begin() + i, v);
    for (i = i + 1; i < operations[op.first].size(); i++) {
      auto e = boost::edge(operations[op.first][i], v, shuffle_graph);
      assert((e.second && shuffle_graph[e.first].state() == blocked));
    }
  }

  for (auto &[edge, processing_order] : operations) {
    assert((!shuffle_graph[processing_order[0]].neighbors_are_valid ||
            !shuffle_graph[processing_order[0]].MP.has_value()));
    for (int i = 1; i < processing_order.size(); i++) {
      if (!shuffle_graph[processing_order[i - 1]].neighbors_are_valid ||
          !shuffle_graph[processing_order[i]].neighbors_are_valid)
        continue;

      assert((shuffle_graph[processing_order[i - 1]].MS->v ==
              processing_order[i]));
      assert((shuffle_graph[processing_order[i]].MP->v ==
              processing_order[i - 1]));

      for (auto &nv : shuffle_graph[processing_order[i]].JP) {
        if (nv.v == prop.src)
          continue;

        assert((shuffle_graph[nv.v].FP.at(processing_order[i]).v ==
                processing_order[i - 1]));
        assert((std::find_if(shuffle_graph[processing_order[i - 1]].FS.begin(),
                             shuffle_graph[processing_order[i - 1]].FS.end(),
                             [&](auto &nv1) { return nv.v == nv1.v; }) !=
                shuffle_graph[processing_order[i - 1]].FS.end()));
      }
    }
    assert((!shuffle_graph[processing_order[processing_order.size() - 1]]
                 .neighbors_are_valid ||
            !shuffle_graph[processing_order[processing_order.size() - 1]]
                 .MS.has_value()));
  }

  for (V v : boost::make_iterator_range(boost::vertices(shuffle_graph))) {
    if (!shuffle_graph[v].neighbors_are_valid)
      continue;

    for (auto &[u, nv] : shuffle_graph[v].FP)
      assert((shuffle_graph[u].MP->v == nv.v));
    for (auto &nv : shuffle_graph[v].FS) {
      bool found = false;
      for (auto &[u, nv1] : shuffle_graph[nv.v].FP)
        found = found || nv.e == nv1.e;
      assert((found));
    }
  }
}

} // namespace tsndgm

#endif // TSN_DGM_SHUFFLE_GRAPH_H
