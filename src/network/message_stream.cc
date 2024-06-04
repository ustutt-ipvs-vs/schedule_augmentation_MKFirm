#include "message_stream.h"
#include "histogram.h"

namespace tsndgm {

void MessageStream::compute_wired_rtis() {
  for (const TreeRouteHop &hop : *route) {
    auto &data_link_property = network->get_data_link_property(hop.edge);

    if (data_link_property.type == wireless) {
      wireless_links.push_back(hop.edge);
      rti_map[hop.edge].add_wireline(
          WIRED_RTI(frame_size, data_link_property.data_rate,
                    data_link_property.propagation_delay, 0)
              .d_trans_max());
    } else {
      auto &device_property = network->get_device_property(hop.edge.second);
      rti_map[hop.edge] = WIRED_RTI(frame_size, data_link_property.data_rate,
                                    data_link_property.propagation_delay,
                                    device_property.processing_delay);
    }
  }
}

Delay MessageStream::compute_effective_deadline(TreeRouteHop &hop) {
  effective_deadline[hop.edge] = e2e_latency;
  for (TreeRouteHop &child : hop.childs) {
    Delay d = compute_effective_deadline(child);
    effective_deadline[hop.edge] = std::min(effective_deadline[hop.edge], d);
  }
  effective_deadline[hop.edge] -= rti_map[hop.edge].d_max();
  return effective_deadline[hop.edge];
}

void MessageStream::compute_effective_release(TreeRouteHop &hop,
                                              Delay release) {
  effective_release[hop.edge] = release;
  for (TreeRouteHop &child : hop.childs) {
    Delay child_release = release + rti_map[hop.edge].d_max();
    compute_effective_release(child, child_release);
  }
}

void MessageStream::initialize() {
  compute_wired_rtis();
  compute_effective_deadline(this->route->root.childs.front());
  compute_effective_release(this->route->root.childs.front(), phase);
}

nlohmann::json MessageStream::dump() const {
  nlohmann::json j = {{"route", {}},
                      {"period", period},
                      {"frame_size", frame_size},
                      {"e2e_latency", e2e_latency},
                      {"rti_map", {}},
                      {"phase", phase},
                      {"jitter", jitter},
                      {"name", name},
                      {"weight", weight}};

  for (TreeRouteHop hop : *route)
    j["route"].push_back(hop.edge);

  for (auto &edge_rti : rti_map) {
    if (network->get_data_link_property(edge_rti.first).type == wired)
      continue;

    j["rti_map"].push_back(
        {{"edge", {edge_rti.first}},
         {"rti",
          {{"d_trans_min", edge_rti.second.d_trans_min()},
           {"d_trans_max", edge_rti.second.d_trans_max()},
           {"d_prop+d_proc",
            edge_rti.second.d_max() - edge_rti.second.d_trans_max()}}}});
  }

  return j;
}

MessageStream::MessageStream(const std::shared_ptr<NetworkTopology> &network,
                             nlohmann::json j) {
  PathRoute path;
  for (auto e : j["route"])
    path.push_back(Edge(e[0], e[1]));

  std::shared_ptr<Route> route = make_shared<Route>(network, std::move(path));

  RTIMap rti_map;
  if (!j["rti_map"].is_null()) {
    for (auto edge_rti : j["rti_map"]) {
      Edge e = Edge(edge_rti["edge"][0], edge_rti["edge"][1]);

      if (!edge_rti["rti"].is_null()) {
        RTI rti(edge_rti["rti"]["d_trans_max"], edge_rti["rti"]["d_trans_min"],
                edge_rti["rti"]["d_prop+d_proc"]);
        rti_map[e] = rti;
      } else if (!edge_rti["reliability"].is_null() &&
                 !edge_rti["histogram"].is_null()) {
        double reliability = edge_rti["reliability"];
        std::string hist_file = edge_rti["histogram"];

        DelayHistogram hist((std::filesystem::path)hist_file);
        RTI rti = hist.compute_rti(reliability);
        rti_map[e] = rti;
      } else {
        throw std::logic_error(
            "Invalid stream file: 'rti_map' entry container either "
            "'rti', or {'reliability', 'histogram'}");
      }
    }
  }

  *this = MessageStream(network, route, j["period"], j["frame_size"],
                        j["e2e_latency"], rti_map, j["phase"], j["jitter"],
                        j["name"], j["weight"]);
}

void dump_streams(const std::vector<MessageStream> &streams,
                  std::filesystem::path out) {
  nlohmann::json j = {};
  for (const MessageStream &stream : streams) {
    j.push_back(stream.dump());
  }

  std::ofstream o(out);
  o << std::setw(4) << j << std::endl;
}

std::vector<MessageStream>
load_streams(const std::shared_ptr<NetworkTopology> &network,
             std::filesystem::path in) {
  std::ifstream i(in);
  nlohmann::json j = nlohmann::json::parse(i);

  std::vector<MessageStream> streams;
  for (auto js : j) {
    streams.push_back(MessageStream(network, js));
  }

  return streams;
}

} // namespace tsndgm
