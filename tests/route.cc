#include <iostream>
#include <vector>

#include "../src/network/route.h"

using namespace std;
using namespace tsndgm;

std::string edge_to_string(const Edge &edge) {
  return "(" + std::to_string(edge.first) + ", " + std::to_string(edge.second) +
         ")";
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

  cout << "----------------------------------------------------" << endl;
  cout << "Contiguous Route Path" << endl;
  cout << "----------------------------------------------------" << endl;

  PathRoute path;
  path.push_back(Edge(1, 2));
  path.push_back(Edge(2, 3));

  Route route(network, std::move(path));
  route.check();
  route.print_route();

  cout << "----------------------------------------------------" << endl;
  cout << "Contiguous TreeRouteHop" << endl;
  cout << "----------------------------------------------------" << endl;

  network->add_data_link(
      make_pair(Edge(2, 4), DataLinkProperty(DataLinkType(1), 999999, 999999)));

  TreeRouteHop root = TreeRouteHop();
  root.add_child(TreeRouteHop(Edge(2, 3)));
  root.add_child(TreeRouteHop(Edge(2, 4)));

  route = Route(network, std::move(root));
  route.check();
  route.print_route();

  cout << "----------------------------------------------------" << endl;
  cout << "Non-Contiguous Route Path" << endl;
  cout << "----------------------------------------------------" << endl;

  path.clear();
  path.push_back(Edge(1, 2));
  path.push_back(Edge(3, 4)); // 3 != 2

  try {
    route = Route(network, std::move(path));
    route.check();
    route.print_route();
    return 1;
  } catch (runtime_error e) {
    std::cout << e.what() << std::endl;
  }

  cout << "----------------------------------------------------" << endl;
  cout << "Non-Contiguous TreeRouteHop" << endl;
  cout << "----------------------------------------------------" << endl;

  root = TreeRouteHop();
  root.add_child(TreeRouteHop(Edge(2, 3)));
  root.add_child(TreeRouteHop(Edge(2, 4)));
  root.childs.front().add_child(TreeRouteHop(Edge(4, 5))); // 4 != 3

  try {
    route = Route(network, std::move(root));
    route.check();
    route.print_route();
    return 1;
  } catch (runtime_error e) {
    std::cout << e.what() << std::endl;
  }

  cout << "----------------------------------------------------" << endl;
  cout << "Multiple Paths" << endl;
  cout << "----------------------------------------------------" << endl;

  network->add_data_link(
      make_pair(Edge(4, 6), DataLinkProperty(DataLinkType(1), 999999, 999999)));

  path.clear();
  path.push_back(Edge(1, 2));
  path.push_back(Edge(2, 3));
  path.push_back(Edge(3, 4));

  route = Route(network, std::move(path));

  path.clear();
  path.push_back(Edge(1, 2));
  path.push_back(Edge(2, 4));
  path.push_back(Edge(4, 6));

  route.add_path(path);
  route.check();
  route.print_route();

  cout << "----------------------------------------------------" << endl;
  cout << "Iterator Test" << endl;
  cout << "----------------------------------------------------" << endl;
  for (const TreeRouteHop &hop : route) {
    cout << edge_to_string(hop.edge) << endl;
  }

  cout << "----------------------------------------------------" << endl;
  cout << "Non-Contiguous Multiple Paths" << endl;
  cout << "----------------------------------------------------" << endl;

  path.clear();
  path.push_back(Edge(1, 2));
  path.push_back(Edge(3, 4)); // 3 != 2
  path.push_back(Edge(4, 6));

  try {
    route.add_path(path);
    route.check();
    return 1;
  } catch (runtime_error e) {
    std::cout << e.what() << std::endl;
  }

  cout << "----------------------------------------------------" << endl;
  cout << "Multiple Paths with different Talkers" << endl;
  cout << "----------------------------------------------------" << endl;

  path.clear();
  path.push_back(Edge(1, 2));
  path.push_back(Edge(2, 3));
  path.push_back(Edge(3, 4));

  route = Route(network, std::move(path));

  path.clear();
  path.push_back(Edge(2, 3)); // 2 != 1
  path.push_back(Edge(3, 4));
  path.push_back(Edge(4, 5));

  try {
    route.add_path(path);
    route.check();
    return 1;
  } catch (runtime_error e) {
    std::cout << e.what() << std::endl;
  }

  cout << "----------------------------------------------------" << endl;
  cout << "Non-Existing Data Link" << endl;
  cout << "----------------------------------------------------" << endl;

  path.clear();
  path.push_back(Edge(1, 2));
  path.push_back(Edge(2, 3));
  path.push_back(Edge(3, 2)); // does not exist

  try {
    route = Route(network, path);
    route.check();
    return 1;
  } catch (runtime_error e) {
    std::cout << e.what() << std::endl;
  }

  return 0;
}
