#include "dgm.h"
#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/copy.hpp>
#include <numeric>

namespace tsndgm
{
    TSNConfiguration DisjunctiveGraphModel::derive_tsn_configuration()
    {
        return TSNConfiguration(transmission_graph, *network);
    }

    CriticalPath::Result
    DisjunctiveGraphModel::critical_path(CriticalPath::Objective type,
                                         bool reverse)
    {
        crit_path.compute_longest_paths(reverse);
        return crit_path.path(type);
    }


    std::pair<Delay, Edge>
    DisjunctiveGraphModel::compute_jitter_bound(MessageStreamHandle ms)
    {
        TransmissionGraphProperty& prop = transmission_graph[boost::graph_bundle];

        Delay max_jitter = 0;
        Edge edge;
        for (auto& listener : prop.streams[ms].route->get_listeners())
        {
            Delay jitter = compute_jitter(ms, listener);
            if (jitter > max_jitter)
            {
                edge = listener;
                max_jitter = jitter;
            }
        }

        return {max_jitter, edge};
    }

    Delay DisjunctiveGraphModel::compute_jitter(MessageStreamHandle ms,
                                                Edge listener)
    {
        TransmissionGraphProperty& prop = transmission_graph[boost::graph_bundle];
        V v_listener = prop.operation_to_vertex[{listener, ms}];

        // at worst (best), ms is transmitted last (first).
        Delay jitter =
            std::accumulate(transmission_graph[v_listener].ms_handle.begin(),
                            transmission_graph[v_listener].ms_handle.end(), (Delay)0,
                            [&](Delay dmax, auto ms1)
                            {
                                return dmax +
                                    prop.streams[ms1].rti_map[listener].d_max();
                            }) -
            prop.streams[ms].rti_map[listener].d_min();
        return jitter;
    }

    void DisjunctiveGraphModel::build()
    {
        TransmissionGraphProperty& prop = transmission_graph[boost::graph_bundle];
        std::ranges::sort(prop.streams,
                          [&](const MessageStream& s1, const MessageStream& s2)
                          {
                              return s1.phase > s2.phase;
                          });

        prop.hyperperiod = 1;
        for (MessageStreamHandle i = 0; i < prop.streams.size(); i++)
        {
            build_stream(i);
            prop.hyperperiod = std::lcm(prop.hyperperiod, prop.streams[i].period);
        }
        resize_properties();
        // TODO update this method by using the given schedule to build the graph
    }

    void DisjunctiveGraphModel::resize_properties()
    {
        TransmissionGraphProperty& prop = transmission_graph[boost::graph_bundle];
        prop.crit_cost.resize(boost::num_vertices(transmission_graph));
        prop.crit_pred.resize(boost::num_vertices(transmission_graph));
        prop.slack.resize(boost::num_vertices(transmission_graph));
    }

    void DisjunctiveGraphModel::build_stream(MessageStreamHandle handle)
    {
        // TODO rewrite this function with the given schedule from the CP
        TransmissionGraphProperty& prop = transmission_graph[boost::graph_bundle];
        for (const TreeRouteHop& hop : *prop.streams[handle].route)
        {
            // add vertex to disjunctive graph
            V v = boost::add_vertex({hop.edge, {handle}, {&hop}}, transmission_graph);
            prop.operation_to_vertex[{hop.edge, handle}] = v;

            // add conjunctive edge from parent
            if (!hop.parent->is_root())
            {
                V v_parent = prop.operation_to_vertex[{hop.parent->edge, handle}];
                boost::add_edge(
                    v_parent, v,
                    {
                        prop.streams[handle].rti_map[hop.parent->edge].d_max(), conjunctive
                    },
                    transmission_graph);
            }
            else
            {
                boost::add_edge(
                    prop.src, v,
                    {prop.streams[handle].phase, conjunctive},
                    transmission_graph);
            }

            // add edge to sink
            if (hop.is_leaf())
            {
                boost::add_edge(v, prop.sink,
                                {
                                    prop.streams[handle].rti_map[hop.edge].d_max(), conjunctive
                                },
                                transmission_graph);
            }

            // add disjunctive edge for every transmission on the same data link
            auto edge_search = prop.edge_to_streams.find(hop.edge);
            if (edge_search == prop.edge_to_streams.end())
            {
                prop.edge_to_streams[hop.edge] = {handle};
            }
            else
            {
                // contesting message streams exist
                for (MessageStreamHandle other : edge_search->second)
                {
                    V u = prop.operation_to_vertex[{hop.edge, other}];
                    TreeRouteHop* u_hop_parent = transmission_graph[u].hop.front()->parent;

                    // add FIFO edges v -> u
                    if (!u_hop_parent->is_root())
                    {
                        Delay weight =
                            prop.streams[handle].rti_map[hop.edge].d_trans_max() -
                            prop.streams[other].rti_map[u_hop_parent->edge].d_min();
                        V u_parent = prop.operation_to_vertex[{u_hop_parent->edge, other}];
                        boost::add_edge(v, u_parent, {weight, fifo},
                                        transmission_graph);
                    }

                    // add disjunctive edge v -> u
                    Delay weight = prop.streams[handle].rti_map[hop.edge].d_trans_max();
                    boost::add_edge(v, u, {weight, disjunctive}, transmission_graph);
                }
                edge_search->second.insert(handle);
            }
        }
    }
} // namespace tsndgm
