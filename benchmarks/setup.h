#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

#include "../src/heuristics/initial.h"
#include "../src/optimization/neighborhood.h"
#include "../src/optimization/tabu_search.h"
#include "../src/optimization/termination.h"

using json = nlohmann::json;
using namespace std;
using namespace tsndgm;
namespace fs = std::filesystem;

void print(DisjunctiveGraphModel &dgm, std::map<int, Edge> &machine_to_datalink,
           ostream &out) {
  auto &prop = dgm.shuffle_graph[boost::graph_bundle];

  out << dgm.critical_path(CriticalPath::Objective::makespan).objective
      << std::endl;
  for (auto &[m, edge] : machine_to_datalink) {
    auto processing_order = dgm.get_processing_order(edge);
    out << "M" << m << ": ";
    for (auto v : processing_order)
      if (dgm.shuffle_graph[v].ms_handle.size() > 1) {
        out << "{";
        bool first = true;
        for (auto ms : dgm.shuffle_graph[v].ms_handle) {
          if (!first)
            out << " ";
          out << ms;
          first = false;
        }
        out << "} ";
      } else {
        out << dgm.shuffle_graph[v].ms_handle.front() << " ";
      }
    out << std::endl;
  }
}

void print(DisjunctiveGraphModel &dgm,
           std::map<int, Edge> &machine_to_datalink) {
  print(dgm, machine_to_datalink, std::cout);
}

void save(DisjunctiveGraphModel &dgm, std::map<int, Edge> &machine_to_datalink,
          std::string subdir, std::string name) {
  fs::create_directories("../data/solutions/" + subdir);

  auto file = fs::path("../data/solutions/" + subdir + "/" + name + ".sol");
  if (fs::exists(file)) {
    ifstream f(file);
    Delay objective;
    f >> objective;
    f.close();

    if (objective <
        dgm.critical_path(CriticalPath::Objective::makespan).objective)
      return;
  }

  ofstream f("../data/solutions/" + subdir + "/" + name + ".sol");
  print(dgm, machine_to_datalink, f);
  f.close();

  std::vector<unsigned int> buf;
  dgm.encode(buf);
  ofstream fenc("../data/solutions/" + subdir + "/" + name + ".enc");
  std::ostream_iterator<unsigned int> output_iterator(fenc, " ");
  std::copy(std::begin(buf), std::end(buf), output_iterator);
  fenc.close();
}

void save_instance(DisjunctiveGraphModel &dgm, std::string name) {
  shuffle_graph_t &shuffle_graph = dgm.shuffle_graph;
  ShuffleGraphProperty &prop = shuffle_graph[boost::graph_bundle];

  fs::create_directories("../data/instances");
  ofstream f("../data/instances/" + name);

  bool first = true;
  for (MessageStream &ms : prop.streams) {
    if (!first)
      f << std::endl;
    bool lfirst = true;
    for (const TreeRouteHop &hop : *ms.route) {
      if (!lfirst)
        f << "; ";
      f << "(" << hop.edge.first << "," << hop.edge.second << "): ["
        << ms.rti_map[hop.edge].d_trans_min() << ","
        << ms.rti_map[hop.edge].d_trans_max() << "]";
      lfirst = false;
    }
    first = false;
  }
  f.close();
}

void set_initial_solution(DisjunctiveGraphModel &dgm, std::string subdir,
                          std::string name) {
  std::vector<unsigned int> buf;
  auto file = fs::path("../data/solutions/" + subdir + "/" + name + ".enc");
  if (!fs::exists(file))
    throw std::runtime_error("file does not exist: " + file.string());
  ifstream fenc(file);

  std::istream_iterator<unsigned int> input_iterator(fenc);
  std::copy(input_iterator, std::istream_iterator<unsigned int>(),
            std::back_inserter(buf));
  dgm.decode(buf);
}

void set_initial_solution(DisjunctiveGraphModel &dgm,
                          std::map<int, Edge> &machine_to_datalink,
                          int machines, int jobs) {
  typedef boost::graph_traits<shuffle_graph_t>::vertex_descriptor V;
  auto &prop = dgm.shuffle_graph[boost::graph_bundle];

  auto file = fs::path("../data/solution");
  if (!fs::exists(file))
    throw std::runtime_error("file does not exist: " + file.string());

  std::ifstream f(file);

  std::map<Edge, std::vector<V>> processing_order;
  for (int i = 0; i < machines; i++) {
    processing_order[machine_to_datalink[i]] = std::vector<V>(jobs);
    auto &machine_processing_order = processing_order[machine_to_datalink[i]];
    for (int j = 0; j < jobs; j++) {
      int job;
      f >> job;
      machine_processing_order[j] =
          prop.operation_to_vertex[{machine_to_datalink[i], job}];
    }
  }

  dgm.apply_processing_order(processing_order);

  dgm.commit_flips();
}

class JSPSetup {
public:
  int jobs, machines;
  std::vector<MessageStream> streams;
  std::shared_ptr<NetworkTopology> network;
  std::map<int, Edge> machine_to_datalink;

  void setup(json &benchmark_data) {
    // read data from benchmark file
    auto file = fs::path("../data/JSPLIB/") /
                fs::path(benchmark_data["path"].template get<std::string>());
    if (!fs::exists(file))
      throw std::runtime_error("file does not exist: " + file.string());

    std::ifstream f(file);
    while (f.peek() == '#')
      f.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    f >> jobs >> machines;
    if (jobs != benchmark_data["jobs"].template get<size_t>() ||
        machines != benchmark_data["machines"].template get<size_t>())
      throw std::runtime_error("instance description and actual data differs");

    int *operations[jobs];
    for (int i = 0; i < jobs; i++) {
      operations[i] = new int[2 * machines];
      for (int j = 0; j < 2 * machines; j++) {
        f >> operations[i][j];
      }
    }

    // convert JSP to TSN problem instance
    vector<NetworkDeviceProperty> device_properties;
    vector<DataLink> data_links;

    for (int i = 0; i < machines; i++) {
      device_properties.push_back(
          NetworkDeviceProperty(2 * i, 0, "M" + std::to_string(i) + "1"));
      DeviceId id1 = device_properties.back().id;
      device_properties.push_back(
          NetworkDeviceProperty(2 * i + 1, 0, "M" + std::to_string(i) + "2"));
      DeviceId id2 = device_properties.back().id;

      machine_to_datalink[i] = Edge(id1, id2);
      data_links.push_back(
          DataLink(Edge(id1, id2), DataLinkProperty(DataLinkType(wireless))));
    }

    network = make_shared<NetworkTopology>(device_properties, data_links);

    for (int i = 0; i < jobs; i++) {
      PathRoute path;
      RTIMap rti_map;
      for (int j = 0; j < 2 * machines; j += 2) {
        jsp_to_tsndgm(operations, i, j, rti_map, path);
      }

      shared_ptr<Route> route = make_shared<Route>(network, std::move(path));
      route->check();
      streams.push_back(MessageStream(network, route, 100, 0, 100, rti_map));
    }
  }

  virtual void jsp_to_tsndgm(int **operations, int i, int j, RTIMap &rti_map,
                             PathRoute &path) {
    Edge edge = machine_to_datalink[operations[i][j]];
    rti_map[edge] = RTI(operations[i][j + 1], operations[i][j + 1]);
    path.push_back(edge);
    auto device =
        NetworkDeviceProperty((2 + i) * machines + j / 2, 0,
                              "D" + std::to_string(i) + std::to_string(j / 2));
    network->add_device(device);
    edge = Edge(edge.second, device.id);
    network->add_data_link(
        DataLink(edge, DataLinkProperty(DataLinkType(wired))));
    path.push_back(edge);
    if (j + 2 < 2 * machines) {
      Edge n_edge = machine_to_datalink[operations[i][j + 2]];
      edge = Edge(edge.second, n_edge.first);
      network->add_data_link(
          DataLink(edge, DataLinkProperty(DataLinkType(wired))));
      path.push_back(edge);
    }
  }

  virtual void save(DisjunctiveGraphModel &dgm,
                    std::map<int, Edge> &machine_to_datalink,
                    std::string name) {
    ::save(dgm, machine_to_datalink, "jsp", name);
  }

  void set_initial_solution(DisjunctiveGraphModel &dgm, std::string name) {
    ::set_initial_solution(dgm, "jsp", name);
  }
};

class JSPwithFIFOSetup : public JSPSetup {
public:
  virtual void jsp_to_tsndgm(int **operations, int i, int j, RTIMap &rti_map,
                             PathRoute &path) {
    Edge edge = machine_to_datalink[operations[i][j]];
    rti_map[edge] = RTI(operations[i][j + 1], operations[i][j + 1]);
    path.push_back(edge);
    if (j + 2 < 2 * machines) {
      Edge n_edge = machine_to_datalink[operations[i][j + 2]];
      edge = Edge(edge.second, n_edge.first);
      network->add_data_link(
          DataLink(edge, DataLinkProperty(DataLinkType(wired))));
      path.push_back(edge);
    }
  }

  void save(DisjunctiveGraphModel &dgm,
            std::map<int, Edge> &machine_to_datalink, std::string name) {
    ::save(dgm, machine_to_datalink, "fifo_jsp", name);
  }

  void set_initial_solution(DisjunctiveGraphModel &dgm, std::string name) {
    ::set_initial_solution(dgm, "fifo_jsp", name);
  }
};

class RobustWirelessTSNSetup {
public:
  std::vector<MessageStream> streams;
  std::shared_ptr<NetworkTopology> network;
  std::map<int, Edge> machine_to_datalink;
  int machines, jobs;

  void setup(json &benchmark_data) {
    machines = 0, jobs = 0;
    // read data from benchmark file
    auto file = fs::path("../data/instances/") /
                fs::path(benchmark_data["name"].template get<std::string>());
    if (!fs::exists(file))
      throw std::runtime_error("file does not exist: " + file.string());

    network = make_shared<NetworkTopology>();

    std::ifstream f(file);
    for (std::string line; getline(f, line);) {
      jobs++, machines = 0;
      PathRoute path;
      RTIMap rti_map;

      size_t pos = 0;
      std::string rti;
      while ((pos = line.find("; ")) != std::string::npos) {
        machines++;
        rti = line.substr(0, pos);
        parse_rti(rti, path, rti_map);
        line.erase(0, pos + 2);
      }
      parse_rti(line, path, rti_map);

      shared_ptr<Route> route = make_shared<Route>(network, std::move(path));
      route->check();
      streams.push_back(MessageStream(network, route, 100, 0, 100, rti_map));
    }
  }

  void parse_rti(std::string &rti, PathRoute &path, RTIMap &rti_map) {
    DeviceId d1 = std::stoi(rti.substr(1, rti.find(",")));
    network->add_device(NetworkDeviceProperty(d1));
    DeviceId d2 = std::stoi(rti.substr(rti.find(",") + 1, rti.find(")")));
    network->add_device(NetworkDeviceProperty(d2));
    Edge edge = Edge(d1, d2);
    path.push_back(edge);
    // to provide a similar output as JSP
    if (d1 % 2 == 0)
      machine_to_datalink[d1 / 2] = edge;
    // even if d_trans_min = d_trans_max, we model every link as wireless,
    // which makes it easier / possible to specify arbitrary RTIs
    network->add_data_link(
        DataLink(edge, DataLinkProperty(DataLinkType(wireless))));
    rti.erase(0, rti.find(": ") + 2);
    Delay d_trans_min = std::stoi(rti.substr(1, rti.find(",")));
    Delay d_trans_max = std::stoi(rti.substr(rti.find(",") + 1, rti.find("]")));
    rti_map[edge] = RTI(d_trans_max, d_trans_min);
  }

  void save(DisjunctiveGraphModel &dgm,
            std::map<int, Edge> &machine_to_datalink, std::string name) {
    ::save(dgm, machine_to_datalink, "robust_wireless_tsn", name);
  }
};
