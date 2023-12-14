#include "transform.h"

namespace tsndgm {

void CriticalBlockTransformSelectionHeuristic::transform(
    CriticalPath::Objective type) {

  ShuffleGraphProperty &prop = dgm.shuffle_graph[boost::graph_bundle];
  auto res = dgm.critical_path(type);

  // randomly select critical block, weighted by their length
  CriticalMachines critical_machines(dgm);
  critical_machines.compute(res);

  std::discrete_distribution<int> d(critical_machines.weights.begin(),
                                    critical_machines.weights.end());
  Edge critical_machine = critical_machines.edges[d(gen)];

  // gather all operations of critical_machine, sorted by crit_cost
  std::set<std::pair<Delay, V>> operation_cost_set;
  for (MessageStreamHandle ms_handle : prop.edge_to_streams[critical_machine]) {
    V v = prop.operation_to_vertex[{critical_machine, ms_handle}];
    operation_cost_set.insert({prop.crit_cost[v], v});

    for (E e :
         boost::make_iterator_range(boost::out_edges(v, dgm.shuffle_graph))) {
      if (dgm.shuffle_graph[e].edge_type == disjunctive) {
        *dgm.shuffle_graph[e].state_pair->state =
            boost::OrientationState::blocked;
      }
    }
  }

  std::vector<V> processing_order;
  for (auto &[d, v] : operation_cost_set) {
    if (processing_order.empty()) {
      processing_order.push_back(v);
      continue;
    }

    // initial processing order
    for (V u : processing_order) {
      *dgm.shuffle_graph[dgm.edge(v, u)].state_pair->state =
          boost::OrientationState::allowed;
    }

    // get processing order with minimal makespan
    std::pair<Delay, int> min_d = {std::numeric_limits<Delay>::max(), 0};
    for (int i = 0; i <= processing_order.size(); i++) {
      bool feasible = true;
      reversed_dgm_traversal(
          dgm.shuffle_graph,
          visitor(feasibility_visitor(dgm.shuffle_graph, feasible))
              .root_vertex(prop.sink));

      if (feasible) {
        auto res = dgm.critical_path(type);
        if (res.objective < min_d.first) {
          dgm.commit_flips();
          min_d = {res.objective, i};
        }
      }

      if (i < processing_order.size()) {
        V u = processing_order[i];
        E e = dgm.edge(v, u);
        dgm.shuffle_graph[e].consistent_flip();
        dgm.flip_log.push_back(e);
      }
    }

    processing_order.insert(processing_order.begin() + min_d.second, v);
    dgm.restore_flips();
  }

  std::cout << " -> Result: "
            << dgm.critical_path(CriticalPath::Objective::makespan).objective
            << std::endl;
}

} // namespace tsndgm
