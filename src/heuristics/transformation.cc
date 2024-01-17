#include "transformation.h"

namespace tsndgm {

void Transformation::print_before_and_after(
    std::map<Edge, std::vector<V>> &before) {
  update_machine_successors();
  for (auto &[edge, b_processing_order] : before) {
    auto a_processing_order = dgm.get_processing_order(edge);
    std::cout << "Machine: (" << edge.first << ", " << edge.second << ")"
              << std::endl;
    std::cout << " -> before:";
    for (V v : b_processing_order)
      std::cout << " " << v;
    std::cout << std::endl;
    std::cout << " -> after:";
    for (V v : a_processing_order)
      std::cout << " " << v;
    std::cout << std::endl;
  }
}

void Transformation::update_machine_successors() {
  // ensure that processing_order is computed correctly
  std::map<V, V> updates;
  reversed_dgm_traversal(
      dgm.shuffle_graph,
      visitor(update_machine_successors_visitor(dgm.shuffle_graph, updates))
          .root_vertex(dgm.shuffle_graph[boost::graph_bundle].sink));
  dgm.update_machine_successors(updates);
}

bool RandomOperationTransformation::accept(
    const std::vector<V> &processing_order, const std::pair<Delay, int> &min_d,
    const CriticalPath::Result &div_result, int v_pos, int cur_pos) {
  if (div_result.objective <
      std::min(min_d.first, int_phase_result.objective)) {
    return true;
  } else if (min_d.first > int_phase_result.objective) {
    double r = d(gen);
    double b = exp(
        -(static_cast<double>(div_result.objective * processing_order.size()) /
          (int_phase_result.objective * abs(v_pos - cur_pos))));
    return r < b;
  }
  return false;
}

bool Transformation::compute_best_permutation(V v) {
  shuffle_graph_t &shuffle_graph = this->dgm.shuffle_graph;
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];

  update_machine_successors();
  dgm.commit_flips();

  auto processing_order = dgm.get_processing_order(v);
  int i = std::find(processing_order.begin(), processing_order.end(), v) -
          processing_order.begin();

  // select best (i.e., minimal objective) position of v in processing order
  std::pair<Delay, int> min_d = {std::numeric_limits<Delay>::max(), -1};
  for (int j = 0; j < processing_order.size(); j++) {
    if (processing_order[j] == v)
      continue;

    dgm.complete_flip(dgm.edge(v, processing_order[j]));
    auto div_result = dgm.critical_path(this->type);

    if (this->accept(processing_order, min_d, div_result, i, j))
      min_d = {div_result.objective, j};

    dgm.restore_flips();
  }

  if (min_d.second != -1)
    dgm.complete_flip(dgm.edge(v, processing_order[min_d.second]));
  return min_d.second != -1;
}

void RandomOperationTransformation::transform(int k) {
  shuffle_graph_t &shuffle_graph = this->dgm.shuffle_graph;
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];

  int_phase_result = dgm.critical_path(this->type);

  int n = boost::num_vertices(shuffle_graph);
  // 0 is src, 1 is sink
  std::uniform_int_distribution<V> d(2, n - 1);
  while (k > 0) {
    V v = d(gen);
    if (prop.edge_to_streams[shuffle_graph[v].edge].size() > 1) {
      compute_best_permutation(v);
      k--;
    }
  }
}

void RandomCriticalPathTransformation::transform(int k) {
  shuffle_graph_t &shuffle_graph = this->dgm.shuffle_graph;
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];

  int_phase_result = dgm.critical_path(this->type);
  std::vector<V> candidates;

  V v = int_phase_result.critical_vertex;
  while (v != prop.src) {
    if (v != prop.sink)
      candidates.push_back(v);
    v = prop.crit_pred[v];
  }

  // Fisher-Yates Shuffle
  for (int i = 0; i < std::min(k, static_cast<int>(candidates.size())); i++) {
    std::uniform_int_distribution<> d(i, candidates.size() - 1);
    int j = d(gen);
    std::swap(candidates[i], candidates[j]);

    V v = candidates[i];
    // only consider interesting operations
    if (prop.edge_to_streams[shuffle_graph[v].edge].size() > 1) {
      compute_best_permutation(v);
    } else {
      k++;
    }
  }
}

auto SlackTransformation::sample_operation() {
  shuffle_graph_t &shuffle_graph = this->dgm.shuffle_graph;
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];

  int_phase_result = dgm.critical_path(this->type);

  Delay slack = 0;
  V v = prop.src, u = prop.sink;
  while (u != v) {
    v = u;
    std::vector<V> candidates = {v};
    std::vector<Delay> candidate_slack = {slack};

    for (auto e :
         boost::make_iterator_range(boost::in_edges(v, shuffle_graph))) {
      V u = source(e, shuffle_graph);
      candidates.push_back(u);
      candidate_slack.push_back(slack + prop.crit_cost[v] -
                                (prop.crit_cost[u] + shuffle_graph[e].weight));
    }

    std::discrete_distribution<> d(candidate_slack.begin(),
                                   candidate_slack.end());
    int i = d(gen);
    u = candidates[i];
    slack = candidate_slack[i];
  }

  return std::make_pair(v, slack);
}

void SlackTransformation::transform(int k) {
  for (int i = 0; i < k; i++) {
    auto sample = sample_operation();
    if (!compute_best_permutation(sample.first)) {
      i--;
    }
  }
}

} // namespace tsndgm
