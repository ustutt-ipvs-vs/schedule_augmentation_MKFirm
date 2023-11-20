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

} // namespace tsndgm
