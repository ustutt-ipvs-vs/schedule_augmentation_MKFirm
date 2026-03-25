# TSN-DGM

When using this code, please cite:

**TODO add reference to our paper**

TSN-DGM is a C++ library for generating robust schedules in wireless Time-Sensitive Networks (TSN) with the Disjunctive Graph Model (DGM).

## Clone Repository
For TSN-DGM itself, it suffices to clone the repository via
```
git clone https://github.com/ustutt-ipvs-vs/schedule_augmentation_MKFirm.git
```
However, if you want to run the JSP benchmarks yourself, the submodule *JSPLIB* must be retrieved as well, e.g., via
```
git submodule update --init --recursive
```
JSPLIB is a collection of JSP benchmarks with known makespan bounds, allowing us to fine-tune TSN-DGM against ``hard'' benchmarking instances. 

## Requirements
- Boost: used to build our shuffle graphs
- nlohmann_json: used to read input data.

## Install requirements
Only tested with Linux / WSL.

Install cmake:
```
sudo apt-get install cmake
```

Install CXX compiler:
```
sudo apt-get install build-essential
```

Install boost:
```
sudo apt-get install libboost-all-dev
```

Install gcc: At least gcc 14 is required.

## Build 
If you wish to modify the compile flags, please modify *CMakeLists.txt*. 
Otherwise, you can simply run:
```
mkdir release
cd release
cmake ..
cmake --build .
```

### Execute

```
./DgmExec -t ../data/topology.json -s ../data/streams.json -z ../data/transmission_output.json --e_streams ../data/emergency_streams.json
```

## Example Usage of TSN-DGM

In the following, we build a simple network ring topology with ten TSN bridges B0-B9, where
 - B0 -> B1 -> B2 -> ... -> B9 are connected by wired links, and
 - B9 -> B0 are connected by a wireless link
We add five message streams that traverse the network from B0 -> B9 via wired links, and one wireless stream that traverses the network from B0 -> B9 via the wireless link.
Thereafter, we provide a template on how to run TSN-DGM.
For additional (more complex) examples, please have a look in *./benchmarks*.

### Build Network Topology
```cpp
std::vector<NetworkDeviceProperty> device_properties;
std::vector<DataLink> data_links;

// add devices
Delay processing_delay = 1000; // in nanoseconds
for (DeviceId bridge_id = 0; bridge_id < 10; bridge_id++) {
  device_properties.push_back(NetworkDeviceProperty(
      bridge_id, processing_delay, "Bt" + std::to_string(bridge_id)));
}

// add data links (wired)
Delay propagation_delay = 50; // in nanoseconds
Delay data_rate = 12500000; // in bytes per second
for (DeviceId b1 = 0; b1 < 9; b1++) {
  DeviceId b2 = b1 + 1;
  data_links.push_back(
      DataLink(Edge(b1, b2), DataLinkProperty(wired, data_rate, propagation_delay)));
}

// add data link (wireless)
bool multiple_subcarriers = true;
data_links.push_back(DataLink(Edge(0, 9), DataLinkProperty(wireless, multiple_subcarriers)));

auto network = make_shared<NetworkTopology>(device_properties, data_links);
```

### Add Message Streams
```cpp
std::vector<MessageStream> message_streams;

// 5 stream traversing B0 -> B1 -> ... -> B9 via wired links
PathRoute wired_path;
for (DeviceId b1 = 0; b1 < 9; b1++) {
  DeviceId b2 = b1 + 1;
  wired_path.push_back(Edge(b1, b2))
}
std::shared_ptr<Route> wired_route = make_shared<Route>(network, std::move(wired_path));
for (int stream = 0; stream < 5; stream++) {
  message_streams.push_back(
      MessageStream(network, wired_route, PERIOD, FRAME_SIZE, DEADLINE);
}

// stream traversing B0 -> B9 via wireless links
PathRoute wireless_path = {Edge(0, 9)};
std::shared_ptr<Route> wireless_route = make_shared<Route>(network, std::move(wired_path));
RTIMap rti_map = {{Edge(0,9), RTI(RTI_MAX, RTI_MIN)}};
message_streams.push_back(
    MessageStream(network, wireless_path, PERIOD, FRAME_SIZE, DEADLINE, rti_map);
```

### Run TSN-DGM
```cpp
TabuSearch tabu_search(network, message_streams);

// set objective to minimize dynamic tardiness (i.e., latency, not deadline) 
auto objective = CriticalPath::Objective::dynamic_tardiness;
auto bound = CriticalPath::get_termination_bound(objective);

/* configure solver with
 ZIPS_TIMEOUT: timeout in seconds for ZIPS solver (e.g. 60)
 TABU_LIST_SIZE: maximum size of tabu list (e.g. 10)
 MAXIT_INT: number of non-improving iterations after which intensification stops (e.g. 500)
 MAXIT_DIV: maximum number of diversification rounds (e.g. 10)
 COMPRESSION_ENABLED: true iff FIPS solver should run after the ZIPS solver terminated
 FIPS_TIMEOUT: timeout in seconds for FIPS solver (e.g. 240)
 SYNC_INTERVAL: interval after which processes should synchronize their elite solutions (e.g. 5)
*/
using InitialHeuristic = EffectiveReleaseInitial;
using TerminationCriterion = TimeoutTerminationCriterion;
using Intensification = StrictAdmissionIntensification<
    DifferentialTerminationCriterion,
    ReducedSelectionCriticalBlockNeighborhood<1>>;
using TransformationHeuristic =
    RandomCriticalPathTransformation<ConstantThenSlowTemperature>;

TabuSearchConfig config{
    objective,
    TerminationConfig(ZIPS_TIMEOUT, bound),
    IntensificationConfig(TABU_LIST_SIZE, MAXIT_INT),
    DiversificationConfig(MAXIT_DIV),
    CompressionConfig(COMPRESSION_ENABLED, TerminationConfig(FIPS_TIMEOUT, bound),
                      IntensificationConfig(TABU_LIST_SIZE, MAXIT_INT/10), SYNC_INTERVAL),
};

tabu_search.run<InitialHeuristic, TerminationCriterion, Intensification,
                TransformationHeuristic>(config);
tabu_search.dgm.print_critical_path(objective);
```

## Building with Unit tests

To build the unit tests the `BUILD_TESTS` property needs to be set in cmake. 

```
mkdir release
cd release
cmake -DBUILD_TESTS=1 ..
cmake --build .
```

To run the tests, run the `unit_tests` executable in the `tests` directory from within the tests directory.

```
cd tests
./unit_tests
```
