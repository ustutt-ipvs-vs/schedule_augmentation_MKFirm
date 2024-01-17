#ifndef TSN_DGM_RELINKING_PHASE_TCC
#define TSN_DGM_RELINKING_PHASE_TCC

#include "relinking_phase.h"

namespace tsndgm {

void Relinking::update_candidates(BestSelection &res) {
  auto it = std::find_if(encoded_best_selections.begin(),
                         encoded_best_selections.end(),
                         [&](auto &s) { return s.objective >= res.objective; });
  EncodedSelection encoded_res(*dgm, res);
  for (auto temp_it = it; temp_it != encoded_best_selections.end() &&
                          (*temp_it).objective == res.objective;
       ++temp_it) {
    if (compute_differing_machines(encoded_res, *it).size() == 0)
      return;
  }
  encoded_best_selections.insert(it, encoded_res);
}

std::pair<int, int> Relinking::sample(RelinkingConfig &config) {
  for (auto it = encoded_best_selections.begin();
       it != encoded_best_selections.end() &&
       it - encoded_best_selections.begin() <= config.max_stored_solutions;) {
    if ((*(it - config.min_stored_solutions)).objective == (*it).objective)
      it = encoded_best_selections.erase(it);
    else
      ++it;
  }

  if (encoded_best_selections.size() > config.max_stored_solutions)
    encoded_best_selections.erase(encoded_best_selections.begin() +
                                      config.max_stored_solutions,
                                  encoded_best_selections.end());

  // randomly select initial and guiding selection for relinking phase
  std::geometric_distribution<int> dg(config.p);
  int initial_index =
      std::min(dg(gen), static_cast<int>(encoded_best_selections.size()) - 2);
  std::uniform_int_distribution<int> du(initial_index + 1,
                                        encoded_best_selections.size() - 1);
  int guiding_index = du(gen);
  return {initial_index, guiding_index};
}

bool Relinking::step_towards(EncodedSelection &guiding_selection,
                             DifferingMachines &differing_machines, int steps) {
  int step;
  for (step = 0; step < steps && !differing_machines.empty();) {
    if (step_towards(guiding_selection, differing_machines))
      step++;
  }
  return step == steps;
}

bool Relinking::step_towards(const EncodedSelection &guiding_selection,
                             DifferingMachines &differing_machines) {
  if (differing_machines.empty())
    return false;

  std::uniform_int_distribution<int> d(0, differing_machines.size() - 1);
  int machine_index = d(gen);
  Edge edge = differing_machines[machine_index].first;
  size_t offset = differing_machines[machine_index].second;

  auto processing_order = dgm->get_processing_order(edge);
  auto guiding_processing_order =
      get_guiding_processing_order(guiding_selection, edge, offset);

  auto get_guiding_pos = [&](DisjunctiveGraphModel::V u) -> int {
    return std::find_if(
               guiding_processing_order.begin(), guiding_processing_order.end(),
               [&](auto &shuffled_ops) { return shuffled_ops.contains(u); }) -
           guiding_processing_order.begin();
  };

  std::vector<DisjunctiveGraphModel::E> candidates;
  for (int i = 0; i < processing_order.size() - 1; i++) {
    DisjunctiveGraphModel::V u = processing_order[i],
                             v = processing_order[i + 1];
    if (get_guiding_pos(u) > get_guiding_pos(v)) {
      candidates.push_back(dgm->edge(u, v));
    }
  }

  if (candidates.empty()) {
    differing_machines.erase(differing_machines.begin() + machine_index);
    return false;
  }

  d = std::uniform_int_distribution<int>(0, candidates.size() - 1);
  int e_index = d(gen);
  dgm->complete_flip(candidates[e_index]);

  return true;
}

Relinking::GuidingMachineProcessingOrder
Relinking::get_guiding_processing_order(
    const EncodedSelection &guiding_selection, Edge edge, int offset) {
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  auto &prop = dgm->shuffle_graph[boost::graph_bundle];

  GuidingMachineProcessingOrder processing_order;

  int len = 3;
  while (offset + len < guiding_selection.buf.size() &&
         guiding_selection.buf[offset + len] != MACHINE_SEPARATOR) {
    if (guiding_selection.buf[offset + len] == SHUFFLE_SEPARATOR) {
      MessageStreamHandle v_ms = guiding_selection.buf[offset + len + 1];
      V v = prop.operation_to_vertex[{edge, v_ms}];
      processing_order.push_back({v});
      len += 2;
      while (guiding_selection.buf[offset + len] != SHUFFLE_SEPARATOR) {
        MessageStreamHandle u_ms = guiding_selection.buf[offset + len];
        V u = prop.operation_to_vertex[{edge, u_ms}];
        if (u != v)
          processing_order.back().insert(u);
        len++;
      }
    } else {
      MessageStreamHandle ms = guiding_selection.buf[offset + len];
      processing_order.push_back({prop.operation_to_vertex[{edge, ms}]});
    }
    len++;
  }

  return processing_order;
}

Relinking::DifferingMachines
Relinking::compute_differing_machines(int initial_index, int guiding_index) {
  EncodedSelection &initial = encoded_best_selections[initial_index],
                   &guiding = encoded_best_selections[guiding_index];
  return compute_differing_machines(initial, guiding);
}

Relinking::DifferingMachines
Relinking::compute_differing_machines(BestSelection &res, int guiding_index) {
  EncodedSelection initial(*dgm, res);
  EncodedSelection &guiding = encoded_best_selections[guiding_index];

  return compute_differing_machines(initial, guiding);
}

Relinking::DifferingMachines
Relinking::compute_differing_machines(EncodedSelection &initial,
                                      EncodedSelection &guiding) {
  DifferingMachines differing_machines;
  int initial_offset = 0, guiding_offset = 0;

  while (initial_offset + 1 < initial.buf.size()) {
    if (initial.buf[initial_offset] != MACHINE_SEPARATOR ||
        guiding.buf[guiding_offset] != MACHINE_SEPARATOR)
      throw std::runtime_error("invalid DGM encoding");

    Edge edge = {initial.buf[initial_offset + 1],
                 initial.buf[initial_offset + 2]};
    assert((edge.first == guiding.buf[guiding_offset + 1] &&
            edge.second == guiding.buf[guiding_offset + 2]));

    int len;
    for (len = 3; initial.buf[initial_offset + len] != MACHINE_SEPARATOR;
         len++) {
      if (initial.buf[initial_offset + len] !=
          guiding.buf[guiding_offset + len]) {
        differing_machines.push_back({edge, guiding_offset});
        break;
      }
    }
    if (initial.buf[initial_offset + len] == MACHINE_SEPARATOR &&
        guiding.buf[guiding_offset + len] != MACHINE_SEPARATOR)
      differing_machines.push_back({edge, guiding_offset});
    initial_offset =
        skip_until(initial, initial_offset + len, MACHINE_SEPARATOR);
    guiding_offset =
        skip_until(guiding, guiding_offset + len, MACHINE_SEPARATOR);
  }

  return differing_machines;
}

} // namespace tsndgm

#endif // TSN_DGM_RELINKING_PHASE_TCC
