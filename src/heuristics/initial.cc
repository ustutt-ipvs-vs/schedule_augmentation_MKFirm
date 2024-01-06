#include "initial.h"

namespace tsndgm {

void RandomInitial::generate() {
  shuffle_graph_t &shuffle_graph = this->dgm.shuffle_graph;
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];

  for (auto &[edge, streams] : prop.edge_to_streams) {
    std::vector<MessageStreamHandle> out;
    std::sample(streams.begin(), streams.end(), std::back_inserter(out),
                streams.size(), gen);
    for (int i = 0; i < out.size() - 1; i++) {
      V u = prop.operation_to_vertex[{edge, out[i]}];
      V v = prop.operation_to_vertex[{edge, out[i + 1]}];
      if (u == v)
        continue;
      dgm.complete_flip(dgm.edge(u, v));
    }
  }
}

void InsertionInitialHeuristic::generate() {
  shuffle_graph_t &shuffle_graph = this->dgm.shuffle_graph;
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];

  // get message stream with largest sum of processing time
  // and get sorted set of operations in order of increasing processing time
  std::vector<Delay> max_ms(prop.streams.size());
  std::set<std::pair<Delay, Operation>> operation_cost_set;
  for (MessageStreamHandle ms_handle = 0; ms_handle < prop.streams.size();
       ms_handle++) {
    MessageStream &ms = prop.streams[ms_handle];
    std::map<const TreeRouteHop *, Delay> proc_path_length;
    proc_path_length[&ms.route->root] = 0;
    for (const TreeRouteHop &hop : *ms.route) {
      proc_path_length[&hop] =
          proc_path_length[hop.parent] + ms.rti_map[hop.edge].d_max();
      operation_cost_set.insert(
          {ms.rti_map[hop.edge].d_max(), {hop.edge, ms_handle}});
      if (hop.is_leaf() && max_ms[ms_handle] < proc_path_length[&hop])
        max_ms[ms_handle] = proc_path_length[&hop];
    }
  }

  std::discrete_distribution<int> d(max_ms.begin(), max_ms.end());
  MessageStreamHandle fixed_ms = d(gen);

  // operation_cost is list of operations with non-increasing processing time
  // processing order maps machines (i.e., edges) to processing order
  std::list<std::pair<Delay, Operation>> operation_cost;
  std::map<Edge, std::vector<MessageStreamHandle>> edge_processing_order;
  for (auto &oc : operation_cost_set) {
    if (oc.second.second != fixed_ms)
      operation_cost.push_front(oc);
    else
      edge_processing_order[oc.second.first] = {fixed_ms};
  }

  // initially, set all disjunctive edges to blocked and clear MP,MS,FP,FS
  for (E e : boost::make_iterator_range(boost::edges(shuffle_graph))) {
    if (shuffle_graph[e].edge_type == disjunctive)
      *shuffle_graph[e].state_pair->state = blocked;
  }
  for (V v : boost::make_iterator_range(boost::vertices(shuffle_graph))) {
    shuffle_graph[v].MP = {};
    shuffle_graph[v].MS = {};
    shuffle_graph[v].FP.clear();
    shuffle_graph[v].FS.clear();
  }

  // incrementally, add operations
  for (auto &oc : operation_cost) {
    auto &[d, o] = oc;
    auto &[e, ms_handle] = o;
    V v = prop.operation_to_vertex[o];

    auto search = edge_processing_order.find(e);
    // o is the first operation on machine e, there's nothing to do
    if (search == edge_processing_order.end()) {
      edge_processing_order[e] = {ms_handle};
      continue;
    }

    // initialize processing order and allow disjunctive edges
    // initially, o is processed before all other operations (hence, allowing v
    // -> u)
    auto &processing_order = edge_processing_order[e];
    V u = prop.operation_to_vertex[{e, processing_order[0]}];
    TreeRouteHop *u_hop_parent = shuffle_graph[u].hop.front()->parent;
    shuffle_graph[v].MS = {u, dgm.edge(v, u)};
    shuffle_graph[u].MP = {v, dgm.edge(v, u)};
    if (!u_hop_parent->is_root()) {
      V u_parent =
          prop.operation_to_vertex[{u_hop_parent->edge, processing_order[0]}];
      shuffle_graph[v].FS.push_back({u_parent, dgm.edge(v, u_parent)});
      shuffle_graph[u_parent].FP[u] = {v, dgm.edge(v, u_parent)};
    }
    for (MessageStreamHandle other : processing_order) {
      V u = prop.operation_to_vertex[{e, other}];
      *shuffle_graph[dgm.edge(v, u)].state_pair->state = allowed;
    }

    // get processing order with minimal objective
    std::pair<Delay, int> min_d = {std::numeric_limits<Delay>::max(), 0};
    for (int i = 0; i <= processing_order.size(); i++) {
      std::map<V, V> updates;
      bool feasible = true;
      reversed_dgm_traversal(
          shuffle_graph,
          visitor(feasibility_visitor(shuffle_graph, updates, feasible))
              .root_vertex(prop.sink));

      if (feasible) {
        dgm.update_machine_successors(updates);
        auto res = dgm.critical_path(type);
        if (res.objective < min_d.first) {
          dgm.commit_flips();
          min_d = {res.objective, i};
        }
      }

      if (i < processing_order.size()) {
        V u = prop.operation_to_vertex[{e, processing_order[i]}];
        E e = dgm.edge(v, u);
        dgm.shuffle_graph[e].consistent_flip(shuffle_graph);
        dgm.flip_log.push_back(e);
      }
    }

    processing_order.insert(processing_order.begin() + min_d.second, ms_handle);
    dgm.restore_flips();
  }
}

void RandomTransformHeuristic::transform(int k) {
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;

  shuffle_graph_t &shuffle_graph = Transformation::dgm.shuffle_graph;
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];

  int n = boost::num_vertices(shuffle_graph);
  // 0 is src, 1 is sink
  std::uniform_int_distribution<V> d(2, n - 1);
  while (k > 0) {
    V v = d(Transformation::gen);
    if (prop.edge_to_streams[shuffle_graph[v].edge].size() > 1) {
      compute_best_permutation(v);
      k--;
    }
  }
}

void RandomTransformHeuristic::generate() {
  RandomInitial::generate();

  Delay best_res = std::numeric_limits<Delay>::max();
  auto res = Transformation::dgm.critical_path(Transformation::type);

  while (res.objective < best_res) {
    best_res = res.objective;
    transform(5);
  }
}

} // namespace tsndgm
