#include "selection_storage.h"

namespace tsndgm {

void SelectionStorage::update_candidates(BestSelection &res) {
  if (res.objective == std::numeric_limits<Delay>::max())
    return;

  auto it = std::find_if(encoded_best_selections.begin(),
                         encoded_best_selections.end(),
                         [&](auto &s) { return s.objective >= res.objective; });
  if (it == encoded_best_selections.end() &&
      encoded_best_selections.size() >= max_stored_solutions)
    return;

  EncodedSelection encoded_res(*dgm, res);
  if (it != encoded_best_selections.end() && it->objective == res.objective)
    std::swap(encoded_res, *it);
  else
    encoded_best_selections.insert(it, encoded_res);

  if (encoded_best_selections.size() > max_stored_solutions)
    encoded_best_selections.erase(encoded_best_selections.begin() +
                                      max_stored_solutions,
                                  encoded_best_selections.end());
}

void SelectionStorage::update_candidates(EncodedSelection &&selection) {
  auto it = std::find_if(
      encoded_best_selections.begin(), encoded_best_selections.end(),
      [&](auto &s) { return s.objective >= selection.objective; });
  if (it == encoded_best_selections.end() &&
      encoded_best_selections.size() >= max_stored_solutions)
    return;

  if (it != encoded_best_selections.end() &&
      it->objective == selection.objective) {
    return;
  } else {
    encoded_best_selections.insert(it, std::move(selection));
  }
}

void SelectionStorage::update_candidates(EncodedSelection &selection) {
  auto it = std::find_if(
      encoded_best_selections.begin(), encoded_best_selections.end(),
      [&](auto &s) { return s.objective >= selection.objective; });
  if (it == encoded_best_selections.end() &&
      encoded_best_selections.size() >= max_stored_solutions)
    return;

  if (it != encoded_best_selections.end() &&
      it->objective == selection.objective) {
    return;
  } else {
    encoded_best_selections.insert(it, selection);
  }
}

void SelectionStorage::delete_candidate(EncodedSelection *res) {
  std::erase_if(encoded_best_selections,
                [&](auto &selection) { return &selection == res; });
}

EncodedSelection &SelectionStorage::sample(double temperature) {
  double p = 0.5 * (1 + temperature);
  std::geometric_distribution<size_t> dg(p);
  return encoded_best_selections[std::min(dg(gen),
                                          encoded_best_selections.size() - 1)];
}

size_t SelectionStorage::get_processing_index(EncodedSelection &selection,
                                              Edge edge,
                                              MessageStreamHandle ms) {
  size_t offset = selection.offset_map[edge];
  assert((edge.first == selection.buf[offset + 1] &&
          edge.second == selection.buf[offset + 2]));

  for (size_t i = offset + 3; selection.buf[i] != MACHINE_SEPARATOR; i++) {
    if (selection.buf[i] == ms)
      return i;
  }

  throw std::runtime_error("message stream not found");
}

void SelectionStorage::set_capacity(size_t max_stored_solutions) {
  this->max_stored_solutions = max_stored_solutions;
}

void SelectionStorage::renew_storage_objectives(CriticalPath::Objective type) {
  for (EncodedSelection &selection : encoded_best_selections) {
    dgm->decode(selection.buf);
    auto res = dgm->critical_path(type);
    selection.objective = res.objective;
    if (res.objective <= CriticalPath::get_termination_bound(type))
      return;
  }
}

} // namespace tsndgm
