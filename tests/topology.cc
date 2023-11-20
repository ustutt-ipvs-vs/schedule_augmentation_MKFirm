#include <iostream>
#include <vector>

#include "../src/network/topology.h"

using namespace std;
using namespace tsndgm;

int main() {
  cout << "----------------------------------------------------" << endl;
  cout << "Network Topology Initialization" << endl;
  cout << "----------------------------------------------------" << endl;

  vector<NetworkDeviceProperty> device_properties;
  vector<DataLink> data_links;
  for (int i = 0; i < 10; i++) {
    device_properties.push_back(UniqueNetworkDeviceProperty(1000));
    data_links.push_back(make_pair(Edge(i, (i + 1) % 10),
                                   DataLinkProperty(DataLinkType(i % 2))));
  }

  NetworkTopology network(device_properties, data_links);

  network.print_topology();

  cout << "----------------------------------------------------" << endl;
  cout << "Network Topology Add Devices & Data Links" << endl;
  cout << "----------------------------------------------------" << endl;

  network.add_device(UniqueNetworkDeviceProperty(999));

  network.add_data_link(
      make_pair(Edge(1, 5), DataLinkProperty(DataLinkType(1), 999999, 999999)));

  data_links.clear();
  for (int i = 0; i < 10; i++)
    data_links.push_back(make_pair(
        Edge(i, (i + 3) % 10),
        DataLinkProperty(DataLinkType(1 - i % 2), 1000000000 + i, i)));
  network.add_data_links(data_links);

  network.print_topology();

  cout << "----------------------------------------------------" << endl;
  cout << "Network Topology Remove Devices & Data Links" << endl;
  cout << "----------------------------------------------------" << endl;

  network.remove_device(1);

  network.print_topology();

  return 0;
}
