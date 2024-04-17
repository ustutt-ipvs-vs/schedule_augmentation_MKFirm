# TSN-DGM

TSN-DGM is a C++ library for generating robust schedules in wireless Time-Sensitive Networks (TSN) with the Disjunctive Graph Model (DGM).

## Clone Repository
For TSN-DGM itself, it suffices to clone the repository via
```
git clone git@github.tik.uni-stuttgart.de:st160727/libtsndgm.git
```
However, if you want to run the JSP benchmarks yourself, the submodule *JSPLIB* must be retrieved as well, e.g., via
```
git submodule update --init --recursive
```
JSPLIB is a collection of JSP benchmarks with known makespan bounds, allowing us to fine-tune TSN-DGM against ``hard'' benchmarking instances. 

## Requirements
- Boost: used to build our shuffle graphs
- OpenMPI: used to exchange elite solutions across multiple processes
- nlohmann_json (optional): used to read JSP benchmark data. If you do not want this dependency, you cannot compile our JSP benchmarks and should disable the *BENCHMARKING* flag during compilation.

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

## Build and Execute Benchmarks  

### Build 
If you wish to modify the compile flags, please modify *CMakeLists.txt*. 
Otherwise, you can simply run:
```
mkdir release
cd release
cmake ..
cmake --build .
```

### Run JSP Benchmarks (Single Process)
After building, you can run the JSP benchmarks via
```
release $ ./benchmarks/jsp 
Usage: ./jsp [0-161] maxt timeout div maxit
```
To benchmark TSN-DGM against ABZ5 with a tabu list size *maxt=8*, a timeout of *timeout=10* seconds, a maximum number of diversification rounds of *div=5*, and a maximum number of non-improving intensification iterations *maxit=15 x MACHINES x JOBS*, run 
```
release $ ./benchmarks/jsp 0 8 10 5 15
Known Optimal Solution: 1234
1238
M0: 8 9 2 1 4 6 3 7 0 5 
M1: 5 6 3 9 2 8 7 0 1 4 
M2: 3 1 7 8 0 4 5 2 6 9 
M3: 4 1 9 8 7 6 3 2 5 0 
M4: 0 7 5 1 4 6 3 8 9 2 
M5: 1 5 0 2 7 8 3 9 4 6 
M6: 8 1 7 0 2 5 9 3 4 6 
M7: 6 3 8 9 2 1 7 5 0 4 
M8: 2 0 9 1 4 5 8 6 7 3 
M9: 2 4 9 0 1 8 5 3 7 6 
Time: 12 s
```
Note that while the total execution time is 12 seconds, the actual ZIPS solver only executes for 10 seconds.
The logs can be seen via
```
release $ tail output_rank0.log 
 Result: 1238 (9s) 
Phase 42:
 Temperature: 0.763; Diversify Rounds: 5; Total Flips: 408061; Total Traversals: 408390
 Storage: 1238 1239 1242 1243 1247 
 Result: 1248 (9s) 
Phase 43:
 Temperature: 0.911; Diversify Rounds: 1; Total Flips: 419543; Total Traversals: 419878
 Storage: 1238 1239 1242 1243 1247 
 Result: 1239 (10s) 
Global Solution: 1238
```

### Run JSP Benchmarks (Multiple Processes)
If you want to employ multiple processes, simply use mpirun
```
release $ mpirun -n 8 ./benchmarks/jsp 0 8 10 5 15
abz5
Known Optimal Solution: 1234
1234
M0: 8 2 9 4 7 6 3 1 0 5 
M1: 5 6 2 7 9 3 8 0 1 4 
M2: 3 7 1 4 5 8 0 6 2 9 
M3: 4 9 1 7 8 6 3 5 2 0 
M4: 7 4 5 0 1 6 3 8 2 9 
M5: 1 5 7 2 0 4 3 8 6 9 
M6: 7 8 1 2 5 0 4 3 9 6 
M7: 6 3 8 7 9 5 2 4 0 1 
M8: 2 4 0 9 5 7 6 1 8 3 
M9: 2 4 9 7 5 0 3 8 6 1 
Time: 12 s

release $ tail output_rank0.log 
 Temperature: 0.0639; Diversify Rounds: 2; Total Flips: 183045; Total Traversals: 183166
 Storage: 1242 1248 
 New Best Selection: 1239 (6s)
 Result: 1242 (6s) 
Phase 16:
 Temperature: 0.0796; Diversify Rounds: 4; Total Flips: 193858; Total Traversals: 193982
 Storage: 1239 1242 1248 
 New Best Selection: 1234 (6s)
 Result: 1234 (6s) 
Global Solution: 1234
```
Again, note that while process 0 found the optimal solution after six seconds, the processes exchange their elite solutions only every few seconds.

### Run Wireless Time-Sensitive Networking Benchmarks
We differentiate between
- cyclic traffic: streams specify their end-to-end latency
- isochronous traffic: streams specify their deadline
To run our benchmarks for cyclic traffic, 
```
release (dev)$ ./benchmarks/graceful_degradation_cyclic 
Usage: ./benchmarks/graceful_degradation_cyclic <talkers> <listeners> <wireless_streams> <cross_traffic> <timeout> <degradation_max> <degradation_step>
```
By specifying *talkers = 3* and *listeners = 3*, the network topology is created with *2^talkers = 8* (*2^listeners = 8*) end-devices on the left (right) network half.
*wireless_streams = 10* sets ten wireless streams that traverse the network from left-to-right and from right-to-left each, i.e., 20 streams in total.
*cross_traffic = 5* sets five cross traffic streams for each network half. With a cross traffic period set to *2ms* (compared to *10ms* for wireless traffic), we have a total cross traffic of *2 x 5 x 10ms/2ms = 50* frames per hyperperiod.
*timeout=240* specifies that the FIPS solver initially has four minutes before the channel degrades. 
Finally, *degradation_max=1500000* and *degradation_step=300000* cover the degradation coverage for mirrored, skewed, and shifted degradation patterns.

The complete example looks like
```
release $ ./benchmarks/graceful_degradation_cyclic 3 3 10 5 240 1500000 300000
```
While running, you may want to follow the progress via *tail -f release/output_rank0.log*.
Of course, you can start the same benchmark with multiple processes as well, e.g., via
```
release $ mpirun -n 8 ./benchmarks/graceful_degradation_cyclic 3 3 10 5 240 1500000 300000
```
