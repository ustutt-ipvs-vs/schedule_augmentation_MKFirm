#include <iostream>
#include <vector>

#include "../src/dgm/critical_path.h"
#include "../src/dgm/dgm.h"

using namespace std;
using namespace tsndgm;

int main() {
  string devices[4] = {"T_1", "T_2", "B_1", "L_1"};
  vector<NetworkDeviceProperty> device_properties;
  for (int i = 0; i < 4; i++)
    device_properties.push_back(UniqueNetworkDeviceProperty(0));
  vector<DataLink> data_links;
  data_links.push_back(
      make_pair(Edge(0, 2), DataLinkProperty(DataLinkType(wireless))));
  data_links.push_back(
      make_pair(Edge(1, 2), DataLinkProperty(DataLinkType(wireless))));
  data_links.push_back(
      make_pair(Edge(2, 3), DataLinkProperty(DataLinkType(wired))));

  shared_ptr<NetworkTopology> network =
      make_shared<NetworkTopology>(device_properties, data_links);

  PathRoute path1 = {Edge(0, 2), Edge(2, 3)};
  shared_ptr<Route> route1 = make_shared<Route>(network, std::move(path1));
  route1->check();

  MessageStream s1(network, route1, 1000, 20, 100);
  s1.rti_map[Edge(0, 2)] = RTI(400, 50, 0);

  PathRoute path2 = {Edge(1, 2), Edge(2, 3)};
  shared_ptr<Route> route2 = make_shared<Route>(network, std::move(path2));
  route2->check();
  MessageStream s2(network, route2, 1000, 20, 1000);
  s2.rti_map[Edge(1, 2)] = RTI(400, 50, 0);

  MessageStream s3(network, route2, 1000, 20, 1000);
  s3.rti_map[Edge(1, 2)] = RTI(55, 50, 0);

  std::vector<MessageStream> streams = {std::move(s1), std::move(s2),
                                        std::move(s3)};

  DisjunctiveGraphModel dgm(network, streams);
  CriticalPath critical_path(dgm.shuffle_graph);

  std::cout << "BEFORE:" << std::endl;
  dgm.print();
  critical_path.compute_longest_paths();
  critical_path.print(critical_path.makespan_path());

  dgm.complete_flip(boost::edge(5, 3, dgm.shuffle_graph).first);
  std::cout << "AFTER:" << std::endl;
  dgm.print();
  critical_path.compute_longest_paths();
  critical_path.print(critical_path.makespan_path());

  dgm.complete_flip(boost::edge(6, 4, dgm.shuffle_graph).first);
  std::cout << "AFTER:" << std::endl;
  dgm.print();
  critical_path.compute_longest_paths();
  critical_path.print(critical_path.makespan_path());

  std::cout << "BACKUP:" << std::endl;
  auto &prop = boost::get_property(dgm.zips_shuffle_graph, boost::graph_bundle);
  print(dgm.zips_shuffle_graph, prop);

  return 0;
}
