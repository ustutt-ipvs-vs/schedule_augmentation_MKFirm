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

    CriticalPath::Result DisjunctiveGraphModel::critical_path(CriticalPath::Objective type, bool reverse)
    {
        crit_path.compute_longest_paths(reverse);
        return crit_path.path(type);
    }

    // not working anymore
    std::pair<Delay, Edge> DisjunctiveGraphModel::compute_jitter_bound(MessageStreamHandle ms)
    {
        TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];

        Delay max_jitter = 0;
        Edge edge;
        for (auto &listener : prop.streams[ms].route->get_listeners())
        {
            Delay jitter = 0;
            if (jitter > max_jitter)
            {
                edge = listener;
                max_jitter = jitter;
            }
        }

        return {max_jitter, edge};
    }

    void DisjunctiveGraphModel::build()
    {
        TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];

        for(const StreamSchedule &current_stream : scheduled_streams){
            for(const FrameSchedule &current_frame_schedule : current_stream.frames){
                add_frame_to_graph(current_frame_schedule, current_stream.stream_id);
            }
        }
    }

    auto get_predecessor_edge(const TransmissionGraphProperty &prop, const MessageStreamHandle other, const Edge &edge)
        -> Edge
    {
        // TODO this function needs to be replaced.
        /* Current issues:
         * the returned value is incorrect if edge = route->get_talker() (if we insert the first edge)
         *
         * the complexity is liniear
         *
         * returns the last edge in case the given edge is not in the path
         */
        const auto &stream = prop.streams[other];
        Edge predecessor = stream.route->get_talker();
        for (const auto &current_edge : stream.route->route)
        {
            if (current_edge.first == edge.first && current_edge.second == edge.second)
            {
                return predecessor;
            }
            predecessor = current_edge;
        }
        return predecessor;
    }

    void DisjunctiveGraphModel::add_frame_to_graph(const FrameSchedule &current_frame_schedule, unsigned int stream_id)
    {
        TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];
        V previous_vertex = prop.src;

        for(const FrameTransmission &current_transmission : current_frame_schedule.transmissions)
        {
            // add vertex to disjunctive graph
            Edge transmission_edge = Edge(current_transmission.source, current_transmission.target);
            V new_vertex = boost::add_vertex({transmission_edge, stream_id, current_frame_schedule.frame_number}, transmission_graph);
            // TODO
            //prop.operation_to_vertex[{transmission_edge, handle}] = new_vertex;

            // add conjunctive edge from parent
            unsigned int weight;
            if(previous_vertex == prop.src){
                // Weight of first conjunctive edge = dRelease (start time / gate open)
                weight = current_transmission.start;
                   
            } 
            else{
                // Weight of inner conjunctive edge = dPropagation + dTransmission + dProcessing (in ns)
                
                Delay dprop = network->get_data_link_property(transmission_edge).propagation_delay;

                DataRate data_rate = network->get_data_link_property(transmission_edge).data_rate;
                FrameSize frame_size = prop.stream_id_map.at(stream_id).frame_size;
                auto factor = frame_size / data_rate;               
                Delay dtrans = static_cast<Delay>(factor * 1e9);

                // Processing delay of next hop (v_f^{k+1} = current_transmission.target) is used
                Delay dproc = network->get_device_property(current_transmission.target).processing_delay;
                
                weight = dprop + dtrans + dproc;
            }
            
            boost::add_edge(previous_vertex, new_vertex, {weight, conjunctive}, transmission_graph); 

            previous_vertex = new_vertex;
        }

        // add edge to sink
        // TODO edge weight
        boost::add_edge(previous_vertex, prop.sink, {0, conjunctive}, transmission_graph);
    }
} // namespace tsndgm
