#include <iostream>
#include <vector>

#include "../src/network/message_stream.h"

using namespace std;
using namespace tsndgm;

string edge_to_string(const Edge &edge) {
  return "(" + to_string(edge.first) + ", " + to_string(edge.second) + ")";
}

string rti_to_string(const RTI &rti) {
  return "[" + to_string(rti.d_min()) + ", " + to_string(rti.d_max()) + "]";
}

int main() {
  vector<NetworkDeviceProperty> device_properties;
  vector<DataLink> data_links;
  for (int i = 0; i < 10; i++) {
    device_properties.push_back(UniqueNetworkDeviceProperty(1000));
    data_links.push_back(make_pair(Edge(i, (i + 1) % 10),
                                   DataLinkProperty(DataLinkType(i % 2))));
  }

  shared_ptr<NetworkTopology> network =
      make_shared<NetworkTopology>(device_properties, data_links);

  PathRoute path = {Edge(1, 2), Edge(2, 3), Edge(3, 4)};
  shared_ptr<Route> route = make_shared<Route>(network, std::move(path));
  route->check();

  MessageStream s1(network, route, 1000, 1518, 100);
  for (const Edge &edge : s1.wireless_links) {
    s1.rti_map[edge] = RTI(4000, 100, 50);
  }

  MessageStream s2(network, route, 1000, 213, 100);
  for (const Edge &edge : s2.wireless_links) {
    s2.rti_map[edge] = RTI(5000, 200, 50);
  }

  cout << "RTIs: Message Stream s1" << endl;
  for (auto &edge_rti : s1.rti_map)
    cout << edge_to_string(edge_rti.first) << ": "
         << rti_to_string(edge_rti.second) << endl;

  cout << "RTIs: Message Stream s2" << endl;
  for (auto &edge_rti : s2.rti_map)
    cout << edge_to_string(edge_rti.first) << ": "
         << rti_to_string(edge_rti.second) << endl;
}
