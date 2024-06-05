#ifndef TSN_DGM_DGM_H
#define TSN_DGM_DGM_H

#include <boost/graph/adjacency_list.hpp>
#include "../network/message_stream.h"
#include "../network/topology.h"
#include "critical_path.h"
#include "transmission_graph.h"
#include "tsn_configuration.h"

namespace tsndgm
{
    typedef std::map<Edge, size_t> OffsetMap;

    class JitterBoundViolation : public std::exception
    {
    public:
        JitterBoundViolation(MessageStreamHandle ms, Edge edge, Delay bound) : ms(ms), edge(edge), bound(bound) {}

        const char *what() { return "jitter exceeds the allowed bound"; }

        MessageStreamHandle ms;
        Edge edge;
        Delay bound;
    };

    class DisjunctiveGraphModel
    {
    public:
        typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;
        typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

        transmission_graph_t transmission_graph;
        std::shared_ptr<NetworkTopology> network;
        CriticalPath crit_path;

        DisjunctiveGraphModel(const std::shared_ptr<NetworkTopology> &network,
                              const std::vector<MessageStream> &streams) :
            network(network), crit_path(transmission_graph)
        {
            transmission_graph[boost::graph_bundle].src = boost::add_vertex(transmission_graph);
            transmission_graph[boost::graph_bundle].sink = boost::add_vertex(transmission_graph);
            transmission_graph[boost::graph_bundle].streams = streams;

            build();
        }

        DisjunctiveGraphModel(const DisjunctiveGraphModel &other) :
            transmission_graph(other.transmission_graph), network(other.network), crit_path(transmission_graph)
        {
        }

        TSNConfiguration derive_tsn_configuration();

        CriticalPath::Result critical_path(CriticalPath::Objective type, bool reverse = true);

        void update_rti(std::map<MessageStreamHandle, RTIMap> rti_updates);

        void split_all();

        std::pair<Delay, Edge> compute_jitter_bound(MessageStreamHandle ms);
        Delay compute_jitter(MessageStreamHandle ms, Edge listener);
        bool apriori_jitter_violation(E uv);

        inline void print() { tsndgm::print(transmission_graph, *network); }

        inline void print(V v) { tsndgm::print(transmission_graph, *network, v); }
        inline void print(E e) { tsndgm::print(transmission_graph, *network, e); }

        inline void print_critical_path(CriticalPath::Objective type)
        {
            crit_path.print(critical_path(type), *network);
        }

        inline void print_fixed_lateness()
        {
            TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];
            for (MessageStreamHandle ms = 0; ms < prop.streams.size(); ms++)
            {
                auto &stream = prop.streams[ms];
                const auto listeners = stream.route->get_listeners();

                // compute tardiness of stream's end-to-end latency
                for (Edge listener : listeners)
                {
                    V v_listener = prop.operation_to_vertex[{listener, ms}];
                    std::cout << ms << ", (" << listener.first << ", " << listener.second << "): " << stream.phase
                              << " " << stream.e2e_latency << " " << prop.crit_cost[v_listener] << " "
                              << crit_path.get_fixed_lateness(ms, listener) << std::endl;
                }
            }
        }

        inline E edge(V u, V v)
        {
            auto e = boost::edge(u, v, transmission_graph);
            if (!e.second)
                throw std::runtime_error("edge (" + std::to_string(u) + ", " + std::to_string(v) + ") does not exist");
            return e.first;
        }

        inline E edge(E uv)
        {
            V u = source(uv, transmission_graph), v = target(uv, transmission_graph);
            return edge(u, v);
        }

    private:
        bool valid_crit_path = false;

        void build();
        void build_stream(MessageStreamHandle handle);
        void resize_properties();
        void update_rti(MessageStreamHandle ms, RTIMap rti_map);

        void internal_commit_all(size_t index);
        void internal_restore_commit(size_t index, bool swap);
        void internal_copy_commit(size_t src_index, size_t dst_index);

        void encode(std::vector<unsigned int> &buf, transmission_graph_t &g, OffsetMap &offset_map);

        void remove_fifo_edges(V u, V v);
        void renew_descriptors();
    };
} // namespace tsndgm

#endif // TSN_DGM_DGM_H
