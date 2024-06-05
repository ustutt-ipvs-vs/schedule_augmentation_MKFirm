#include "route.h"
#include <iostream>

namespace tsndgm {

std::string edge_to_string(const Edge &edge) {
  return "(" + std::to_string(edge.first) + ", " + std::to_string(edge.second) +
         ")";
}

void TreeRouteHop::add_path(const PathRoute &route) {
  TreeRouteHop cur, *prev = this;

  for (auto &route_edge : route) {
    cur.edge = route_edge;
    cur.parent = prev;
    auto it = std::find(prev->childs.begin(), prev->childs.end(), cur);
    if (it != std::end(prev->childs)) {
      prev = &(*it);
    } else {
      prev->childs.push_back(std::move(cur));
      prev = &prev->childs.back();
    }
  }
}

void MulticastRoute::check() {
  if (valid)
    return;

  visited_hops.clear();
  std::list<TreeRouteHop *> hops;
  std::set<Edge> visited_edges;

  // check if talkers match
  DeviceId talker = root.childs.front().edge.first;
  for (TreeRouteHop &child : root.childs) {
    if (child.edge.first != talker)
      throw std::runtime_error{
          "talkers do not match: " + std::to_string(talker) + " and " +
          std::to_string(child.edge.first)};
    if (!visited_edges.insert(child.edge).second)
      throw std::runtime_error("Route contains at least two different paths "
                               "that use the same data link");
    hops.push_back(&child);
    visited_hops.push_back(child);
    child.parent = &root; // ensure parent pointers are correct
  }

  // check if all paths are contiguous
  while (!hops.empty()) {
    TreeRouteHop *next = hops.front();

    if (!network->exists(next->edge))
      throw std::runtime_error{"data link does not exist: " +
                               edge_to_string(next->edge)};

    for (TreeRouteHop &child : next->childs) {
      if (next->edge.second != child.edge.first)
        throw std::runtime_error{
            "route is not contiguous: " + edge_to_string(next->edge) + " -> " +
            edge_to_string(child.edge)};
      if (!visited_edges.insert(child.edge).second)
        throw std::runtime_error("Route contains at least two different paths "
                                 "that use the same data link");
      hops.push_back(&child);
      visited_hops.push_back(child);
      child.parent = next; // ensure parent pointers are correct
    }
    hops.pop_front();
  }
  valid = true;
}

void MulticastRoute::print_route() {
  std::list<TreeRouteHop *> hops;
  for (TreeRouteHop &child : root.childs)
    hops.push_back(&child);

  while (!hops.empty()) {
    TreeRouteHop *next = hops.front();
    std::cout << edge_to_string(next->edge) << " ";
    for (TreeRouteHop &child : next->childs)
      hops.push_back(&child);
    hops.pop_front();
  }
  std::cout << std::endl;
}

void MulticastRoute::compute_talker_and_listeners() {
  talker = root.childs.front().edge;
  for (const TreeRouteHop &hop : *this) {
    if (hop.is_leaf())
      listeners.push_back(hop.edge);
  }
}

} // namespace tsndgm
