#include "initial.h"

namespace tsndgm {

void RandomInitial::generate(CriticalPath::Objective type) {
  shuffle_graph_t &shuffle_graph = this->dgm.shuffle_graph;
  std::discrete_distribution<int> d({1, 1});
  std::set<boost::OrientationState *> flipped_edges;
  std::list<E> edge_list;

  for (E e : boost::make_iterator_range(boost::edges(shuffle_graph))) {
    if (shuffle_graph[e].edge_type == disjunctive &&
        shuffle_graph[e].state() == boost::OrientationState::allowed &&
        d(gen) == 1 &&
        !flipped_edges.contains(shuffle_graph[e].state_pair->state.get())) {
      edge_list.push_back(e);
      flipped_edges.insert(shuffle_graph[e].state_pair->state.get());
    }
  }

  dgm.complete_flip(edge_list);
}

void INSA::generate(CriticalPath::Objective type) {
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

  // initially, set all disjunctive edges to blocked
  for (E e : boost::make_iterator_range(boost::edges(shuffle_graph))) {
    if (shuffle_graph[e].edge_type == disjunctive)
      *shuffle_graph[e].state_pair->state = boost::OrientationState::blocked;
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
    for (MessageStreamHandle other : processing_order) {
      V u = prop.operation_to_vertex[{e, other}];
      *shuffle_graph[dgm.edge(v, u)].state_pair->state =
          boost::OrientationState::allowed;
    }

    // get processing order with minimal makespan
    std::pair<Delay, int> min_d = {std::numeric_limits<Delay>::max(), 0};
    for (int i = 0; i <= processing_order.size(); i++) {
      bool feasible = true;
      reversed_dgm_traversal(
          shuffle_graph, visitor(feasibility_visitor(shuffle_graph, feasible))
                             .root_vertex(prop.sink));

      if (feasible) {
        auto res = dgm.critical_path(type);
        if (res.objective < min_d.first) {
          dgm.commit_flips();
          min_d = {res.objective, i};
        }
      }

      if (i < processing_order.size()) {
        V u = prop.operation_to_vertex[{e, processing_order[i]}];
        E e = dgm.edge(v, u);
        shuffle_graph[e].consistent_flip();
        dgm.flip_log.push_back(e);
      }
    }

    processing_order.insert(processing_order.begin() + min_d.second, ms_handle);
    dgm.restore_flips();
  }
}

} // namespace tsndgm
