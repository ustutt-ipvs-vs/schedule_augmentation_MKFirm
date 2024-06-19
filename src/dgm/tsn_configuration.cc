#include "tsn_configuration.h"

namespace tsndgm {
void TSNConfiguration::compute() {
  TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];
  reversed_dgm_traversal(transmission_graph,
                         visitor(tsn_configuration_visitor(dgm, gcl_config, psfp_config)).root_vertex(prop.sink));

  // extend GCL to entire hyperperiod
  for (auto &entry : gcl_config) {
    PeriodicGate &gate = entry.second;
    gate.offset = prop.hyperperiod - std::accumulate(std::begin(gate.durations), std::end(gate.durations), 0.0);
    gate.durations.front() += gate.offset;
  }

  // compute initial transmission offsets
  for (MessageStreamHandle ms = 0; ms < prop.tt_streams.size(); ms++) {
    MessageStream stream = prop.tt_streams[ms];
    Edge talker = stream.route.get_talker();
    auto v = prop.operation_to_vertex[{talker, ms}];

    initial_transmission_config[ms] = prop.crit_cost[v];
  }
}

void TSNConfiguration::dump(std::filesystem::path out) {
  TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];
  nlohmann::json j = {
      {"INIT", nlohmann::json::object()}, {"GCL", nlohmann::json::object()}, {"PSFP", nlohmann::json::object()}};

  // dump initial transmission offsets
  for (auto &[ms, d] : initial_transmission_config) {
    if (prop.tt_streams[ms].name != "") {
      j["INIT"][prop.tt_streams[ms].name] = initial_transmission_config[ms];
    } else {
      j["INIT"][ms] = initial_transmission_config[ms];
    }
  }

  // dump GCL
  for (auto &entry : gcl_config) {
    const Edge &edge = entry.first;
    const PeriodicGate &gate = entry.second;

    std::string egress_port = "[" + topology.get_device_property(edge.first).name + "," +
                              topology.get_device_property(edge.second).name + "]";
    j["GCL"][egress_port]["initial"] = gate.initial;
    j["GCL"][egress_port]["offset"] = gate.offset;
    j["GCL"][egress_port]["durations"] = nlohmann::json::array();

    for (Delay d : gate.durations) {
      j["GCL"][egress_port]["durations"].push_back(d);
    }
  }

  // dump PSFP
  for (auto &entry : psfp_config) {
    DeviceId v = entry.first;
    std::string v_name = topology.get_device_property(v).name;
    j["PSFP"][v_name] = nlohmann::json::array();

    auto psfp_gates = entry.second;
    for (auto &psfp_gate : psfp_gates) {
      size_t i = j["PSFP"][v_name].size();
      j["PSFP"][v_name].push_back(
          {{"streams", nlohmann::json::array()}, {"open", psfp_gate.open}, {"close", psfp_gate.close}});
      for (auto ms : psfp_gate.streams) {
        if (prop.tt_streams[ms].name != "") {
          j["PSFP"][v_name][i]["streams"].push_back(prop.tt_streams[ms].name);
        } else {
          j["PSFP"][v_name][i]["streams"].push_back(ms);
        }
      }
    }
  }

  std::ofstream o(out);
  o << std::setw(4) << j << std::endl;
}

// not working anymore
void tsn_configuration_visitor::finish_vertex(V v, const transmission_graph_t &transmission_graph) {
  if (v == prop.src || v == prop.sink)
    return;

  // add entry to GCL config
  Edge egress_port = transmission_graph[v].edge;
  DataRate rate = topology.get_data_link_property(egress_port).data_rate;
  Delay prop_delay = topology.get_data_link_property(egress_port).propagation_delay;
  PeriodicGate &gate = gcl_config[egress_port];

  Delay open_duration = 0;
  Delay d_trans_min = std::numeric_limits<Delay>::max();
  Delay d_trans_total = 0;
  /*
  for (MessageStreamHandle ms : transmission_graph[v].ms_handle) {
    // GCL only covers wireline portion
    // If link is wireless, it only covers the wireless portion between switch
    // and the radio link
    open_duration +=
        RTI(prop.streams[ms].frame_size, rate, prop_delay).d_trans_max();
    d_trans_total += prop.streams[ms].rti_map[egress_port].d_trans_max();
    if (prop.streams[ms].rti_map[egress_port].d_trans_min() < d_trans_min)
      d_trans_min = prop.streams[ms].rti_map[egress_port].d_trans_min();
  }
  */

  if (!last_op.contains(egress_port))
    last_op[egress_port] = 0;
  if (prop.crit_cost[v] == last_op[egress_port] && last_op[egress_port] > 0) {
    gate.durations.back() += open_duration;
  } else {
    gate.durations.push_back(prop.crit_cost[v] - last_op[egress_port]);
    gate.durations.push_back(open_duration);
  }
  last_op[egress_port] = prop.crit_cost[v] + open_duration;

  // add entry to PSFP config
  std::list<PSFPGate> &psfp_gates = psfp_config[egress_port.second];
  Delay proc_delay = topology.get_device_property(egress_port.second).processing_delay;
  /*
  psfp_gates.push_back(
      PSFPGate(transmission_graph[v].ms_handle,
              prop.crit_cost[v] + d_trans_min + proc_delay,
              prop.crit_cost[v] + d_trans_total + proc_delay));
  */
}
} // namespace tsndgm
