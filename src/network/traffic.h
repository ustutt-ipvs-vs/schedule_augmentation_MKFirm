#ifndef TSN_DGM_TRAFFIC_H
#define TSN_DGM_TRAFFIC_H

#include "topology.h"

namespace tsndgm {

typedef unsigned int MessageStreamIdType_t;

struct PathRoute {};

struct TreeRoute {};

struct EdgeListRoute {};

struct MessageStream {};

class NetworkTraffic {
public:
  NetworkTraffic(NetworkTopology network);
};

} // namespace tsndgm

#endif // TSN_DGM_TOPOLOGY_H
