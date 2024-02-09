#include "initial.h"

namespace tsndgm {

void RandomInitial::generate() {
  shuffle_graph_t &shuffle_graph = this->dgm.shuffle_graph;
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];

  for (auto &[edge, streams] : prop.edge_to_streams) {
    std::vector<MessageStreamHandle> out;
    std::copy(streams.begin(), streams.end(), std::back_inserter(out));
    std::shuffle(out.begin(), out.end(), gen);
    for (int i = 0; i < out.size() - 1; i++) {
      if (prop.streams[out[i]].phase + prop.streams[out[i]].e2e_latency <=
          prop.streams[out[i + 1]].phase)
        continue;

      V u = prop.operation_to_vertex[{edge, out[i]}];
      V v = prop.operation_to_vertex[{edge, out[i + 1]}];
      if (u == v)
        continue;
      dgm.complete_flip(dgm.edge(u, v));
    }
  }
}

} // namespace tsndgm
