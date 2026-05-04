# Augmentation Procedure for the (m,k)-firm Elevation Policy

This repository provides the general augmentation procedure, using a TSN schedule as an input and prolonging/deferring its transmission slots to account for the additional delays induced by elevated traffic.
For a more tightly coupled integration with a modern TSN scheduler (and an improved implementation of transmission graphs), you may want to look at our [integration with FIPS](https://github.com/ustutt-ipvs-vs/FIPS_MKFirm).

This repository is part of the paper _An (m, k)-firm Elevation Policy for Weakly Hard  Real-Time in Converged 5G-TSN Networks_.
For more context please refer to this document: https://doi.org/10.5281/zenodo.19224732

When using this code, please cite:

**TODO add reference to our paper**

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

