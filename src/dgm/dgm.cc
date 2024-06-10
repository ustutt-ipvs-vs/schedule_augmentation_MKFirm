#include "dgm.h"
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/copy.hpp>

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

    void DisjunctiveGraphModel::build()
    {
        TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];

        for (const StreamSchedule &current_stream : scheduled_streams)
        {
            for (const FrameSchedule &current_frame_schedule : current_stream.frames)
            {
                add_conjunctive_edge_for_frame(current_frame_schedule, current_stream.stream_id);
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
        Edge predecessor = stream.route.get_talker();
        for (const auto &current_edge : stream.route.route)
        {
            if (current_edge.first == edge.first && current_edge.second == edge.second)
            {
                return predecessor;
            }
            predecessor = current_edge;
        }
        return predecessor;
    }

    void DisjunctiveGraphModel::add_conjunctive_edge_for_frame(const FrameSchedule &current_frame_schedule, StreamID stream_id)
    {
        TransmissionGraphProperty &prop = transmission_graph[boost::graph_bundle];
        V previous_vertex = prop.src;

        unsigned int weight_previous_iteration;
        for (const FrameTransmission &current_transmission : current_frame_schedule.transmissions)
        {
            // add vertex to disjunctive graph
            auto transmission_edge = Edge(current_transmission.source, current_transmission.target);
            const V new_vertex = boost::add_vertex({transmission_edge, stream_id, current_frame_schedule.frame_number},
                                                   transmission_graph);
            // TODO
            // prop.operation_to_vertex[{transmission_edge, handle}] = new_vertex;
            unsigned int weight;

            // Special case first iteration: add conjunctive edge from parent
            if (previous_vertex == prop.src)
            {
                // Weight of first conjunctive edge = dRelease (start time / gate open)
                weight_previous_iteration = current_transmission.start;
                
            }

            // Add edge, use weight calculated in previous iteration
            boost::add_edge(previous_vertex, new_vertex, {weight_previous_iteration, conjunctive}, transmission_graph);
                        
            
            // Calculate weight for next iteration = dPropagation + dTransmission + dProcessing (in ns)
            const Delay dprop = network->get_data_link_property(transmission_edge).propagation_delay;

            const DataRate data_rate = network->get_data_link_property(transmission_edge).data_rate;
            const FrameSize frame_size = prop.stream_id_map.at(stream_id).frame_size;
                
            const double factor = frame_size * 1.0e9L;
            const Delay dtrans = static_cast<Delay>(factor / data_rate);

            // Processing delay of next hop (v_f^{k+1} = current_transmission.target) is used
            const Delay dproc = network->get_device_property(current_transmission.target).processing_delay;

            weight_previous_iteration = dprop + dtrans + dproc;
            previous_vertex = new_vertex;
        }

        // add edge to sink using last weight calculated in loop
        boost::add_edge(previous_vertex, prop.sink, {weight_previous_iteration, conjunctive}, transmission_graph);
    }
} // namespace tsndgm
