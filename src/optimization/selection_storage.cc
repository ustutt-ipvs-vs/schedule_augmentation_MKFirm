#include "selection_storage.h"

namespace tsndgm {

void SelectionStorage::update_candidates(BestSelection &res) {
  {
    const std::lock_guard<std::mutex> lock(candidate_mutex);
    for (EncodedSelection &candidate : candidates)
      update_candidates(std::move(candidate), true);
    candidates.clear();
  }

  auto it = std::find_if(encoded_best_selections.begin(),
                         encoded_best_selections.end(),
                         [&](auto &s) { return s.objective >= res.objective; });
  if (it == encoded_best_selections.end() &&
      encoded_best_selections.size() >= max_stored_solutions)
    return;

  EncodedSelection encoded_res(dgm, res);
  if (it != encoded_best_selections.end() && it->objective == res.objective)
    std::swap(encoded_res, *it);
  else
    encoded_best_selections.insert(it, encoded_res);

  if (encoded_best_selections.size() > max_stored_solutions)
    encoded_best_selections.erase(encoded_best_selections.begin() +
                                      max_stored_solutions,
                                  encoded_best_selections.end());
}

void SelectionStorage::update_candidates(EncodedSelection &&selection,
                                         bool direct_update) {
  auto it = std::find_if(
      encoded_best_selections.begin(), encoded_best_selections.end(),
      [&](auto &s) { return s.objective >= selection.objective; });
  if (it == encoded_best_selections.end() &&
      encoded_best_selections.size() >= max_stored_solutions)
    return;

  if (it != encoded_best_selections.end() &&
      it->objective == selection.objective) {
    return;
  } else if (direct_update) {
    encoded_best_selections.insert(it, std::move(selection));
  } else {
    const std::lock_guard<std::mutex> lock(candidate_mutex);
    candidates.push_back(std::move(selection));
  }
}

EncodedSelection &SelectionStorage::sample(double temperature) {
  double p = 0.5 * (1 - temperature);
  std::geometric_distribution<size_t> dg(p);
  return encoded_best_selections[std::min(dg(gen),
                                          encoded_best_selections.size() - 1)];
}

size_t SelectionStorage::get_processing_index(EncodedSelection &selection,
                                              Edge edge,
                                              MessageStreamHandle ms) {
  size_t offset = selection.offset_map[edge];
  for (size_t i = offset + 3; selection.buf[i] != MACHINE_SEPARATOR; i++) {
    if (selection.buf[i] == ms)
      return i;
  }

  throw std::runtime_error("message stream not found");
}

} // namespace tsndgm
