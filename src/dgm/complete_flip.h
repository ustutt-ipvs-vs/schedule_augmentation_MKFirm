#ifndef TSN_DGM_COMPLETE_FLIP_H
#define TSN_DGM_COMPLETE_FLIP_H

#include "shuffle_graph.h"
#include "traversal.h"

namespace tsndgm {

template <typename Graph>
class complete_flip_visitor : public boost::default_dfs_visitor {
public:
  typedef boost::graph_traits<Graph>::vertex_descriptor Vertex;
  typedef boost::graph_traits<Graph>::edge_descriptor Edge;

  enum State {
    not_visited_flipped_edge,
    visited_flipped_edge,
    visited_flipped_edge_first
  };

  complete_flip_visitor(const std::set<OrientationStatePair *> &flipped_edges,
                        std::list<Edge> &required_flips)
      : flipped_edges(flipped_edges), required_flips(required_flips) {}

  void back_edge(Edge e, const Graph &g) {
    auto search = candidate_map.find(source(e, g));
    if (search == candidate_map.end())
      throw std::runtime_error(
          "Cyclic route for message stream with handle " +
          std::to_string(g[source(e, g)].ms_handle.front()));
    if (search->second.second == not_visited_flipped_edge)
      throw std::runtime_error(
          "Shuffle graph contained cycle before complete flip");
    else if (search->second.second == visited_flipped_edge)
      required_flips.push_back(search->second.first);
    else
      required_flips.push_back(e);
  }

  void examine_edge(Edge e, const Graph &g) {
    auto search = candidate_map.find(source(e, g));
    if (search != candidate_map.end()) {
      if (search->second.second != not_visited_flipped_edge ||
          g[e].edge_type == conjunctive)
        candidate_map[target(e, g)] = search->second;
      else if (flipped_edges.contains(g[e].state_pair.get()))
        candidate_map[target(e, g)] =
            std::make_pair(search->second.first, visited_flipped_edge);
      else
        candidate_map[target(e, g)] =
            std::make_pair(e, not_visited_flipped_edge);
    } else {
      if (flipped_edges.contains(g[e].state_pair.get()))
        candidate_map[target(e, g)] =
            std::make_pair(e, visited_flipped_edge_first);
      else if (g[search->second.first].edge_type != conjunctive)
        candidate_map[target(e, g)] =
            std::make_pair(e, not_visited_flipped_edge);
    }
  }

  const std::set<OrientationStatePair *> &flipped_edges;
  std::map<Vertex, std::pair<Edge, State>> candidate_map;
  std::list<Edge> &required_flips;
};

} // namespace tsndgm

#endif // TSN_DGM_COMPLETE_FLIP_H
