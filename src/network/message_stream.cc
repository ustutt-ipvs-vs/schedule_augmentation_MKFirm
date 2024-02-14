#include "message_stream.h"

namespace tsndgm {

void MessageStream::compute_wired_rtis() {
  for (const TreeRouteHop &hop : *route) {
    auto &data_link_property = network->get_data_link_property(hop.edge);

    if (data_link_property.type == wireless) {
      wireless_links.push_back(hop.edge);
    } else {
      rti_map[hop.edge] = WIRED_RTI(frame_size, data_link_property.data_rate,
                                    data_link_property.propagation_delay);
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

} // namespace tsndgm
