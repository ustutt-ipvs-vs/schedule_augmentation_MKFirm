#include "initial.h"

namespace tsndgm {

void RandomInitial::generate() {
  transmission_graph_t &transmission_graph = this->dgm.transmission_graph;
  TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];

  for (auto &[edge, streams] : prop.edge_to_streams) {
    std::vector<MessageStreamHandle> out;
    std::copy(streams.begin(), streams.end(), std::back_inserter(out));
    std::shuffle(out.begin(), out.end(), gen);
    for (int i = 0; i < out.size() - 1; i++) {
      V u = prop.operation_to_vertex[{edge, out[i]}];
      V v = prop.operation_to_vertex[{edge, out[i + 1]}];
      if (u == v)
        continue;
      dgm.complete_flip(dgm.edge(u, v));
    }
  }
}

void EffectiveReleaseInitial::generate() {
  transmission_graph_t &transmission_graph = dgm.transmission_graph;
  TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];

  for (auto &[edge, _] : prop.edge_to_streams) {
    auto processing_order = dgm.get_processing_order(edge);
    std::sort(processing_order.begin(), processing_order.end(),
              [&](auto &v, auto &v1) {
                auto ms = transmission_graph[v].ms_handle.front();
                auto ms1 = transmission_graph[v1].ms_handle.front();
                return prop.streams[ms].effective_release[edge] <
                       prop.streams[ms1].effective_release[edge];
              });
    for (int i = 0; i < processing_order.size(); i++) {
      V u = processing_order[i];
      for (int j = i + 1; j < processing_order.size(); j++) {
        V v = processing_order[j];
        E uv = dgm.edge(u, v);
        if (transmission_graph[uv].state() == blocked) {
          dgm.complete_flip(uv);
        }
      }
    }
  }
}

} // namespace tsndgm
