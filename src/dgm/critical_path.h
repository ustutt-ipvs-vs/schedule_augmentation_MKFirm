#ifndef TSN_DGM_CRITICAL_PATH_H
#define TSN_DGM_CRITICAL_PATH_H

#include "transmission_graph.h"
#include "traversal.h"

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

        void examine_edge(E e, const transmission_graph_t& transmission_graph) const
        {
            assert((transmission_graph[e].state() == allowed));
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

    class update_machine_successors_visitor : public longest_path_visitor
    {
    public:
        typedef boost::graph_traits<transmission_graph_t>::vertex_descriptor V;

        update_machine_successors_visitor(transmission_graph_t& transmission_graph,
                                          std::map<V, V>& updated_machine_successors)
            : longest_path_visitor(transmission_graph),
              updated_machine_successors(updated_machine_successors)
        {
        }

        void discover_vertex(V v, const transmission_graph_t& transmission_graph) const
        {
            longest_path_visitor::discover_vertex(v, transmission_graph);

            if (!transmission_graph[v].neighbors_are_valid)
            {
                auto find = updated_machine_successors.find(v);
                if (find == updated_machine_successors.end() ||
                    ((*find).second != 0 &&
                        transmission_graph[boost::edge(v, (*find).second, transmission_graph).first]
                        .state() == blocked))
                    updated_machine_successors[v] = 0;
            }
        }

        void examine_edge(E e, const transmission_graph_t& transmission_graph) const
        {
            assert((transmission_graph[e].state() == allowed));

            if (transmission_graph[e].edge_type == conjunctive)
                return;
            e = fifo_to_disjunctive_edge(e, transmission_graph);

            V u = source(e, transmission_graph), v = target(e, transmission_graph);
            if (transmission_graph[u].neighbors_are_valid)
                return;

            auto find = updated_machine_successors.find(u);
            if (find == updated_machine_successors.end() || (*find).second == 0 ||
                (*find).second == v ||
                transmission_graph[boost::edge((*find).second, v, transmission_graph).first]
                .state() == blocked ||
                transmission_graph[boost::edge(u, (*find).second, transmission_graph).first]
                .state() == blocked)
            {
                updated_machine_successors[u] = v;
            }
        }

        inline E
        fifo_to_disjunctive_edge(E uv, const transmission_graph_t& transmission_graph) const
        {
            if (transmission_graph[uv].edge_type == disjunctive)
            {
                return uv;
            }
            else if (transmission_graph[uv].edge_type == fifo)
            {
                V u = source(uv, transmission_graph), v = target(uv, transmission_graph);
                for (const NeighborVertex& JS : transmission_graph[v].JS)
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

        std::map<V, V>& updated_machine_successors;
    };

    class feasibility_visitor : public update_machine_successors_visitor
    {
    public:
        feasibility_visitor(transmission_graph_t& transmission_graph,
                            std::map<V, V>& updated_machine_successors,
                            bool& feasible)
            : update_machine_successors_visitor(transmission_graph,
                                                updated_machine_successors),
              feasible(feasible)
        {
        }

        bool back_edge(E e, const transmission_graph_t& transmission_graph)
        {
            feasible = false;
            return true; // aborts traversal
        }

    private:
        bool& feasible;
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

        void examine_edge(E e, const transmission_graph_t& transmission_graph) const
        {
            assert((transmission_graph[e].state() == allowed));
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
            fixed_lateness,
            dynamic_lateness,
            weighted_fixed_lateness,
            weighted_dynamic_lateness,
            fixed_tardiness,
            dynamic_tardiness,
            weighted_fixed_tardiness,
            weighted_dynamic_tardiness
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
            case fixed_tardiness:
            case dynamic_tardiness:
            case weighted_fixed_tardiness:
            case weighted_dynamic_tardiness:
                return 0;
            case fixed_lateness:
            case dynamic_lateness:
                return std::numeric_limits<Delay>::min();
            case weighted_fixed_lateness:
            case weighted_dynamic_lateness:
                return -100;
            default:
                throw std::logic_error("type does not exist: " + std::to_string(type));
            }
        }

        void compute_longest_paths(bool reverse = true);

        Result path(Objective type);
        Result makespan_path();
        Result fixed_lateness_path(Delay min = 0);
        Result weighted_fixed_lateness_path(Delay min = 0);
        Result dynamic_lateness_path(Delay min = 0);
        Result weighted_dynamic_lateness_path(Delay min = 0);

        Delay get_fixed_lateness(MessageStreamHandle ms, Edge listener);
        double get_weighted_fixed_lateness(MessageStreamHandle ms, Edge listener);
        Delay get_dynamic_lateness(MessageStreamHandle ms, Edge listener);
        double get_weighted_dynamic_lateness(MessageStreamHandle ms, Edge listener);

        void print(Result res, const NetworkTopology& network);

    private:
        transmission_graph_t& transmission_graph;
    };
} // namespace tsndgm

#endif // TSN_DGM_CRITICAL_PATH_H
