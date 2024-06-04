#ifndef TSN_DGM_CRITICAL_PATH_H
#define TSN_DGM_CRITICAL_PATH_H

#include "transmission_graph.h"
#include <boost/graph/depth_first_search.hpp>

namespace tsndgm
{
    class longest_path_visitor : public boost::default_dfs_visitor
    {
    public:
        static int total_traversals;
        typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;
        typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

        longest_path_visitor(transmission_graph_t& transmission_graph)
            : prop(boost::get_property(transmission_graph, boost::graph_bundle))
        {
            total_traversals++;
        }

        virtual bool back_edge(E e, const transmission_graph_t& transmission_graph) const
        {
            throw std::runtime_error(
                "Selection is not complete; disjunctive graph is acyclic.");
        }

        void discover_vertex(V v, const transmission_graph_t& transmission_graph) const
        {
            prop.crit_cost[v] = 0;
            if (reversed)
            {
                prop.crit_pred[v] = prop.src;
            }
            else
            {
                prop.crit_pred[v] = prop.sink;
            }
        }

        void finish_edge(E uv, const transmission_graph_t& transmission_graph) const
        {
            V u, v;
            if (reversed)
            {
                u = source(uv, transmission_graph), v = target(uv, transmission_graph);
            }
            else
            {
                v = source(uv, transmission_graph), u = target(uv, transmission_graph);
            }
            Delay v_cost = prop.crit_cost[v];
            Delay u_cost = prop.crit_cost[u] + transmission_graph[uv].weight;
            if (u_cost >= v_cost)
            {
                prop.crit_cost[v] = u_cost;
                prop.crit_pred[v] = u;
            }
        }

        TransmissionGraphProperty& prop;
        bool reversed = true;
    };


    class slack_visitor : public boost::default_dfs_visitor
    {
    public:
        typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;
        typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

        slack_visitor(transmission_graph_t& transmission_graph)
            : prop(boost::get_property(transmission_graph, boost::graph_bundle))
        {
            longest_path_visitor::total_traversals++;
        }

        virtual bool back_edge(E e, const transmission_graph_t& transmission_graph) const
        {
            throw std::runtime_error(
                "Selection is not complete; disjunctive graph is acyclic.");
        }

        void discover_vertex(V v, const transmission_graph_t& transmission_graph) const
        {
            if (v == prop.sink || boost::edge(v, prop.sink, transmission_graph).second)
                prop.slack[v] = 0;
            else
                prop.slack[v] = std::numeric_limits<Delay>::max();
        }

        void finish_edge(E uv, const transmission_graph_t& transmission_graph) const
        {
            V u = source(uv, transmission_graph), v = target(uv, transmission_graph);

            if (transmission_graph[uv].weight == std::numeric_limits<Delay>::min())
                return;

            Delay uv_slack =
                prop.crit_cost[v] - prop.crit_cost[u] - transmission_graph[uv].weight;
            if (uv_slack + prop.slack[v] < prop.slack[u])
                prop.slack[u] = uv_slack + prop.slack[v];
        }

        TransmissionGraphProperty& prop;
        bool reversed = false;
    };

    class CriticalPath
    {
    public:
        typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;
        typedef boost::graph_traits<transmission_graph_t>::edge_descriptor E;

        enum Objective
        {
            makespan,
            fixed_lateness, // TODO rename -> Deadline
            dynamic_lateness, // TODO rename -> e2e latency
            fixed_tardiness,
            dynamic_tardiness,
        };

        struct Result
        {
            Delay objective;
            V critical_vertex;
        };

        CriticalPath(transmission_graph_t& transmission_graph) : transmission_graph(transmission_graph)
        {
        }

        CriticalPath& operator=(const CriticalPath& other)
        {
            if (this != &other)
            {
                transmission_graph = other.transmission_graph;
            }
            return *this;
        }

        static Delay get_termination_bound(Objective type)
        {
            switch (type)
            {
            case makespan:
                [[fallthrough]]
            case fixed_tardiness:
                [[fallthrough]]
            case dynamic_tardiness:
                return 0;
            case fixed_lateness:
                [[fallthrough]]
            case dynamic_lateness:
                return std::numeric_limits<Delay>::min();
            default:
                throw std::logic_error("type does not exist: " + std::to_string(type));
            }
        }

        void compute_longest_paths(bool reverse = true);

        Result path(Objective type);
        Result makespan_path();
        Result fixed_lateness_path(Delay min = 0);
        Result dynamic_lateness_path(Delay min = 0);

        Delay get_fixed_lateness(MessageStreamHandle ms, Edge listener);
        Delay get_dynamic_lateness(MessageStreamHandle ms, Edge listener);

        void print(Result res, const NetworkTopology& network);

    private:
        transmission_graph_t& transmission_graph;
    };
} // namespace tsndgm

#endif // TSN_DGM_CRITICAL_PATH_H
