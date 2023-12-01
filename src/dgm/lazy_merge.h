#ifndef TSN_DGM_LAZY_MERGE_H
#define TSN_DGM_LAZY_MERGE_H

#include "complete_flip.h"
#include "shuffle_graph.h"
#include "traversal.h"

namespace tsndgm {

template <typename Graph>
class lazy_merge_visitor : public complete_flip_visitor<Graph> {
public:
  typedef boost::graph_traits<Graph>::vertex_descriptor Vertex;
  typedef boost::graph_traits<Graph>::edge_descriptor Edge;

  lazy_merge_visitor(const std::set<Vertex> &merged_vertices,
                     const std::set<boost::OrientationState *> &flipped_edges,
                     std::list<Edge> &required_flips)
      : complete_flip_visitor<Graph>(flipped_edges, required_flips),
        merged_vertices(merged_vertices) {}

  void discover_vertex(Vertex v, const Graph &g) {
    if (merged_vertices.contains(v)) {
      auto search = this->candidate_map.find(v);
      if (search != this->candidate_map.end()) {
        if (search->second.second ==
            complete_flip_visitor<Graph>::not_visited_flipped_edge)
          this->candidate_map[v] = std::make_pair(
              search->second.first,
              complete_flip_visitor<Graph>::visited_flipped_edge);
      } else {
      }
    }
  }

  const std::set<Vertex> &merged_vertices;
};

} // namespace tsndgm

#endif // TSN_DGM_LAZY_MERGE_H
