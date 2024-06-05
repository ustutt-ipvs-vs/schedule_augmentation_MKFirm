#ifndef TSN_DGM_ROUTE_H
#define TSN_DGM_ROUTE_H

#include <iostream>
#include "topology.h"

namespace tsndgm
{

    typedef std::vector<Edge> PathRoute;

    /** \brief Represents a hop in the network.
     *
     * A hop is a data link between two network devices.
     * For the hop [v,w], TreeRouteHop saves the parent hop [u,v] and all childs
     * hops [w,x], [w,y], ...
     */
    struct TreeRouteHop
    {
        Edge edge; /**< Data link */
        TreeRouteHop *parent = nullptr; /**< Parent hop */
        std::list<TreeRouteHop> childs; /**< Child hops */

        TreeRouteHop(const Edge &edge) : edge(edge) {}
        TreeRouteHop(Edge &&edge) : edge(std::move(edge)) {}

        TreeRouteHop(const PathRoute &route) { add_path(route); }

        TreeRouteHop() = default;
        TreeRouteHop(TreeRouteHop &&hop) = default;
        TreeRouteHop(const TreeRouteHop &hop) = default;

        TreeRouteHop &operator=(const TreeRouteHop &other) = default;
        TreeRouteHop &operator=(TreeRouteHop &&other) = default;

        /** \brief Check if the hop is a root hop.
         * \return true if the hop is a root hop, false otherwise.
         */
        bool is_root() const { return parent == nullptr; }

        /** \brief Check if the hop is a leaf hop.
         * \return true if the hop is a leaf hop, false otherwise.
         */
        bool is_leaf() const { return childs.empty(); }

        /** \brief Add child hop.
         * \param child child hop.
         */
        template <class T>
        void add_child(T &&child)
        {
            childs.push_back(std::forward<T>(child));
        }

        /** \brief Add a complete path to the route.
         * \param route path to add.
         */
        void add_path(const PathRoute &route);

        bool operator==(const TreeRouteHop &other) const
        {
            return edge.first == other.edge.first && edge.second == other.edge.second;
        }
    };

    /** \brief Represents a route in the network.
     *
     * A route is a tree of data links.
     * For the root node [u,v], the network device u is the talker.
     * Analogous, for a leaf node [u,v], the network device v is the listener.
     */
    class MulticastRoute
    {
    private:
        std::shared_ptr<NetworkTopology> network;
        std::list<std::reference_wrapper<const TreeRouteHop>> visited_hops;
        bool valid = false;

        Edge talker;
        std::list<Edge> listeners;

        void compute_talker_and_listeners();

    public:
        TreeRouteHop root; /**< Root hop of the route. */

        /** \brief Construct a route from a network topology and a root hop.
         * \param network shared pointer to network topology.
         * \param root root hop.
         */
        template <class T>
        MulticastRoute(const std::shared_ptr<NetworkTopology> &network, T &&root) :
            network(network), root(std::forward<T>(root))
        {
            check();
            compute_talker_and_listeners();
        }

        MulticastRoute(MulticastRoute &&other) = default;
        MulticastRoute(const MulticastRoute &other) = default;

        MulticastRoute &operator=(const MulticastRoute &other) = default;
        MulticastRoute &operator=(MulticastRoute &&other) = default;

        /** \brief Add a path to the route.
         * \param route path to add.
         */
        void add_path(const PathRoute &route)
        {
            valid = false;
            root.add_path(route);
            check();
            compute_talker_and_listeners();
        }

        /** \brief Check if the route is valid.
         *
         * A route is valid if the following holds true:
         * - Every hop exists in the network topology
         * - Every hop is connected to its parent hop (i.e. hops are of the form
         * [u,v], [v,w], [w,x], ...)
         * - Every path shares the same talker
         */
        void check();

        inline Edge get_talker()
        {
            if (!valid)
                compute_talker_and_listeners();
            return talker;
        }
        inline const std::list<Edge> &get_listeners()
        {
            if (!valid)
                compute_talker_and_listeners();
            return listeners;
        }

        void print_route();

        using iterator = decltype(visited_hops)::const_iterator;
        iterator begin() const { return visited_hops.begin(); }
        iterator end() const { return visited_hops.end(); }
    };

    struct Route
    {
        DeviceId source;
        DeviceId destination;

        PathRoute route;

        explicit Route(PathRoute &&input_route)
        {
            route = std::move(input_route);
            source = route.front().first;
            destination = route.back().second;
        }

        auto print_route() const -> void
        {
            std::cout << "Route from " << source << " to " << destination << ": ";
            for (const auto &[from, to] : route)
            {
                std::cout << from << " -> " << to << " -> ";
            }
            std::cout << std::endl;
        }

        auto get_talker() -> Edge { return route.front(); }

        auto get_listeners() -> std::vector<Edge> { return {route.back()}; }
    };

} // namespace tsndgm

#endif // TSN_DGM_ROUTE_H
