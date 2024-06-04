#ifndef TSN_DGM_DGM_H
#define TSN_DGM_DGM_H

#include "../network/message_stream.h"
#include "../network/topology.h"
#include "critical_path.h"
#include "transmission_graph.h"
#include "traversal.h"
#include "tsn_configuration.h"
#include <boost/graph/adjacency_list.hpp>

namespace tsndgm
{
#define MACHINE_SEPARATOR static_cast<unsigned int>(-1)
#define SHUFFLE_SEPARATOR static_cast<unsigned int>(-2)

    typedef std::map<Edge, size_t> OffsetMap;

    class JitterBoundViolation : public std::exception
    {
    public:
        JitterBoundViolation(MessageStreamHandle ms, Edge edge, Delay bound)
            : ms(ms), edge(edge), bound(bound)
        {
        }

        const char* what() { return "jitter exceeds the allowed bound"; }

        MessageStreamHandle ms;
        Edge edge;
        Delay bound;
    };

    class DisjunctiveGraphModel
    {
    public:
        typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;
        typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

        typedef std::vector<V> MachineProcessingOrder;
        typedef std::map<Edge, MachineProcessingOrder> ProcessingOrder;

        unsigned long total_flips = 0;
        transmission_graph_t transmission_graph;
        std::shared_ptr<NetworkTopology> network;
        CriticalPath crit_path;
        std::list<E> flip_log;

        DisjunctiveGraphModel(const std::shared_ptr<NetworkTopology>& network,
                              const std::vector<MessageStream>& streams)
            : network(network), crit_path(transmission_graph)
        {
            transmission_graph[boost::graph_bundle].src = boost::add_vertex(transmission_graph);
            transmission_graph[boost::graph_bundle].sink = boost::add_vertex(transmission_graph);
            transmission_graph[boost::graph_bundle].streams = streams;

            build();

            transmission_graph[boost::graph_bundle].is_zips_selection = true;
        }

        DisjunctiveGraphModel(const DisjunctiveGraphModel& other)
            : transmission_graph(other.transmission_graph), flip_log(other.flip_log),
              network(other.network),
              committed_transmission_graphs(other.committed_transmission_graphs),
              crit_path(transmission_graph)
        {
        }

        TSNConfiguration derive_tsn_configuration();

        CriticalPath::Result critical_path(CriticalPath::Objective type,
                                           bool reverse = true);

        void update_rti(std::map<MessageStreamHandle, RTIMap> rti_updates);

        void complete_flip(std::list<E>& edges, bool combined = true);
        void complete_flip(E e);

        void complete_shuffle(const std::list<E>& edges, bool commit_fallback = true,
                              bool fix_cycles = true);
        void complete_shuffle(E e, bool commit_fallback = true,
                              bool fix_cycles = true);

        inline void undo_last_shuffle()
        {
            internal_restore_commit(shuffle_fallback, false);
        }

        void split_all();

        std::pair<Delay, Edge> compute_jitter_bound(MessageStreamHandle ms);
        Delay compute_jitter(MessageStreamHandle ms, Edge listener);
        bool apriori_jitter_violation(E uv);

        inline void commit_flips() { flip_log.clear(); }

        inline void restore_flips()
        {
            restore_flips(flip_log.size());
            // update_machine_successors();
        }

        void restore_flips(size_t n);

        inline void commit_all(size_t index = 0)
        {
            internal_commit_all(index + EXTERNAL_COMMIT_OFFSET);
        }

        inline void restore_commit(size_t index = 0, bool swap = false)
        {
            internal_restore_commit(index + EXTERNAL_COMMIT_OFFSET, swap);
        }

        inline void copy_commit(size_t src_index, size_t dst_index)
        {
            internal_copy_commit(src_index + EXTERNAL_COMMIT_OFFSET,
                                 dst_index + EXTERNAL_COMMIT_OFFSET);
        }

        inline void encode(std::vector<unsigned int>& buf)
        {
            OffsetMap offset_map;
            encode(buf, offset_map);
        }

        inline void encode(std::vector<unsigned int>& buf, OffsetMap& offset_map)
        {
            encode(buf, transmission_graph, offset_map);
        }

        inline void encode(std::vector<unsigned int>& buf, int index)
        {
            OffsetMap offset_map;
            encode(buf, index, offset_map);
        }

        inline void encode(std::vector<unsigned int>& buf, int index,
                           OffsetMap& offset_map)
        {
            encode(buf, committed_transmission_graphs[index + EXTERNAL_COMMIT_OFFSET],
                   offset_map);
        }

        void decode(std::vector<unsigned int>& buf);

        inline void apply_processing_order(const ProcessingOrder& processing_order)
        {
            for (auto& [edge, operations] : processing_order)
                apply_machine_processing_order(operations);
        };
        void apply_machine_processing_order(const MachineProcessingOrder& operations);
        void update_machine_successors();
        void update_machine_successors(std::map<V, V> updates);

        inline MachineProcessingOrder get_processing_order(Edge edge)
        {
            return get_processing_order(transmission_graph, edge);
        }

        inline MachineProcessingOrder get_processing_order(V v)
        {
            return get_processing_order(transmission_graph, v);
        }

        inline ProcessingOrder get_processing_order()
        {
            return get_processing_order(transmission_graph);
        }

        MachineProcessingOrder get_processing_order(transmission_graph_t& g, Edge edge);
        MachineProcessingOrder get_processing_order(transmission_graph_t& g, V v);

        inline ProcessingOrder get_processing_order(transmission_graph_t& g)
        {
            ProcessingOrder processing_order;
            for (auto& [edge, _] : g[boost::graph_bundle].edge_to_streams)
            {
                processing_order[edge] = get_processing_order(edge);
            }
            return processing_order;
        }

        inline void print() { tsndgm::print(transmission_graph, *network); }
        inline void print(V v) { tsndgm::print(transmission_graph, *network, v); }
        inline void print(E e) { tsndgm::print(transmission_graph, *network, e); }

        inline void print_critical_path(CriticalPath::Objective type)
        {
            crit_path.print(critical_path(type), *network);
        }

        inline void print_fixed_lateness()
        {
            TransmissionGraphProperty& prop = transmission_graph[boost::graph_bundle];
            for (MessageStreamHandle ms = 0; ms < prop.streams.size(); ms++)
            {
                auto& stream = prop.streams[ms];
                const std::list<Edge>& listeners = stream.route->get_listeners();

                // compute tardiness of stream's end-to-end latency
                for (Edge listener : listeners)
                {
                    V v_listener = prop.operation_to_vertex[{listener, ms}];
                    std::cout << ms << ", (" << listener.first << ", " << listener.second
                        << "): " << stream.phase << " " << stream.e2e_latency << " "
                        << prop.crit_cost[v_listener] << " "
                        << crit_path.get_fixed_lateness(ms, listener) << std::endl;
                }
            }
        }

        inline void print_dynamic_lateness()
        {
            TransmissionGraphProperty& prop = transmission_graph[boost::graph_bundle];
            dgm_traversal(transmission_graph,
                          visitor(slack_visitor(transmission_graph)).root_vertex(prop.src));
            for (MessageStreamHandle ms = 0; ms < prop.streams.size(); ms++)
            {
                auto& stream = prop.streams[ms];
                const std::list<Edge>& listeners = stream.route->get_listeners();
                Edge talker = stream.route->get_talker();
                V v_talker = prop.operation_to_vertex[{talker, ms}];

                // compute tardiness of stream's end-to-end latency
                for (Edge listener : listeners)
                {
                    V v_listener = prop.operation_to_vertex[{listener, ms}];
                    std::cout << ms << ", (" << listener.first << ", " << listener.second
                        << "): " << stream.phase << " " << stream.e2e_latency << " "
                        << prop.crit_cost[v_talker] << " " << prop.slack[v_talker]
                        << " " << prop.crit_cost[v_listener] << " "
                        << crit_path.get_dynamic_lateness(ms, listener) << std::endl;
                }
            }
        }

        inline void print_weighted_fixed_lateness()
        {
            TransmissionGraphProperty& prop = transmission_graph[boost::graph_bundle];
            for (MessageStreamHandle ms = 0; ms < prop.streams.size(); ms++)
            {
                auto& stream = prop.streams[ms];
                const std::list<Edge>& listeners = stream.route->get_listeners();

                // compute tardiness of stream's end-to-end latency
                for (Edge listener : listeners)
                {
                    V v_listener = prop.operation_to_vertex[{listener, ms}];
                    std::cout << ms << ", (" << listener.first << ", " << listener.second
                        << "): " << stream.phase << " " << stream.e2e_latency << " "
                        << prop.crit_cost[v_listener] << " "
                        << crit_path.get_weighted_fixed_lateness(ms, listener)
                        << std::endl;
                }
            }
        }

        inline void print_weighted_dynamic_lateness()
        {
            TransmissionGraphProperty& prop = transmission_graph[boost::graph_bundle];
            dgm_traversal(transmission_graph,
                          visitor(slack_visitor(transmission_graph)).root_vertex(prop.src));
            for (MessageStreamHandle ms = 0; ms < prop.streams.size(); ms++)
            {
                auto& stream = prop.streams[ms];
                const std::list<Edge>& listeners = stream.route->get_listeners();
                Edge talker = stream.route->get_talker();
                V v_talker = prop.operation_to_vertex[{talker, ms}];

                // compute tardiness of stream's end-to-end latency
                for (Edge listener : listeners)
                {
                    V v_listener = prop.operation_to_vertex[{listener, ms}];
                    std::cout << ms << ", (" << listener.first << ", " << listener.second
                        << "): " << stream.phase << " " << stream.e2e_latency << " "
                        << prop.crit_cost[v_talker] << " " << prop.slack[v_talker]
                        << " " << prop.crit_cost[v_listener] << " "
                        << crit_path.get_weighted_dynamic_lateness(ms, listener)
                        << std::endl;
                }
            }
        }

        inline E edge(V u, V v)
        {
            auto e = boost::edge(u, v, transmission_graph);
            if (!e.second)
                throw std::runtime_error("edge (" + std::to_string(u) + ", " +
                    std::to_string(v) + ") does not exist");
            return e.first;
        }

        inline E edge(E uv)
        {
            V u = source(uv, transmission_graph), v = target(uv, transmission_graph);
            return edge(u, v);
        }

        inline E rev_edge(E uv)
        {
            V u = source(uv, transmission_graph), v = target(uv, transmission_graph);
            if (transmission_graph[uv].edge_type == disjunctive)
            {
                return edge(v, u);
            }
            else if (transmission_graph[uv].edge_type == fifo)
            {
                // for fifo edges, vu does not exist
                for (NeighborVertex& JS : transmission_graph[v].JS)
                {
                    auto e = boost::edge(JS.v, u, transmission_graph);
                    if (e.second)
                    {
                        return e.first;
                    }
                }
                // this should never happen
                throw std::runtime_error("shuffle graph is invalid");
            }

            throw std::runtime_error("operation not supported for edges of type: " +
                std::to_string(transmission_graph[uv].edge_type));
        }

        inline E fifo_to_disjunctive_edge(E uv)
        {
            if (transmission_graph[uv].edge_type == disjunctive)
            {
                return uv;
            }
            else if (transmission_graph[uv].edge_type == fifo)
            {
                V u = source(uv, transmission_graph), v = target(uv, transmission_graph);
                for (NeighborVertex& JS : transmission_graph[v].JS)
                {
                    auto e = boost::edge(u, JS.v, transmission_graph);
                    if (e.second)
                    {
                        return e.first;
                    }
                }
                // this should never happen
                throw std::runtime_error("shuffle graph is invalid");
            }
            throw std::runtime_error("operation not supported for edges of type: " +
                std::to_string(transmission_graph[uv].edge_type));
        }

    private:
        bool valid_crit_path = false;

        /** CommitIndices is used to name the internal usage of commit indices.
         * initial: fallback: used in the split_all operation to undo all shuffles
         * shuffle_fallback: used if shuffling results in an UnfixableCycleExcpetion
         * EXTERNAL_OFFSET: so that external commits do not interfere
         */
        enum CommitIndices { initial, shuffle_fallback, EXTERNAL_COMMIT_OFFSET };

        std::vector<transmission_graph_t> committed_transmission_graphs;

        void build();
        void build_stream(MessageStreamHandle handle);
        void resize_properties();
        void update_rti(MessageStreamHandle ms, RTIMap rti_map);

        void internal_commit_all(size_t index);
        void internal_restore_commit(size_t index, bool swap);
        void internal_copy_commit(size_t src_index, size_t dst_index);

        void encode(std::vector<unsigned int>& buf, transmission_graph_t& g,
                    OffsetMap& offset_map);

        // If !uv.has_value(), complete_flip eliminates cycles with
        // at least one edge in flipped_edges
        void complete_flip(std::set<OrientationState*>& flipped_edges,
                           const std::set<V>& shuffled_operations,
                           std::optional<E> uv);
        void complete_shuffle(E e, std::set<OrientationState*>& flipped_edges,
                              std::set<V>& shuffled_operations,
                              bool fix_cycles = true);

        void remove_fifo_edges(V u, V v);
        void renew_descriptors();
    };
} // namespace tsndgm

#endif // TSN_DGM_DGM_H
