#ifndef TSN_DGM_TRAVERSAL_H
#define TSN_DGM_TRAVERSAL_H

#include <boost/graph/depth_first_search.hpp>

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "transmission_graph.h"

namespace boost
{
    namespace detail
    {
        /** \brief Modified depth_first_visit_impl (from
         * boost/graph/depth_first_search.hpp) for the disjunctive graph model.
         */
        template <class BidirectionalGraph, class DGMVisitor, class ColorMap,
                  class TerminatorFunc, bool reversed>
        void dgm_visit_impl(
            const BidirectionalGraph& g,
            typename graph_traits<BidirectionalGraph>::vertex_descriptor u,
            DGMVisitor& vis, ColorMap color, TerminatorFunc func = TerminatorFunc())
        {
            //  ! BEGIN MODIFICATION
            //  Verify that the graph is a bidirectional graph, allowing access to both
            //  in- and out-edges. Previously, DFS used IncidenceGraphConcept
            BOOST_CONCEPT_ASSERT((BidirectionalGraphConcept<BidirectionalGraph>));
            // Verify that the edge property has a member "state" of type OrientationState
            BOOST_CONCEPT_ASSERT(
                (boost::Convertible<decltype(std::declval<typename edge_bundle_type<
                        BidirectionalGraph>::type>()
                        .state()),
                    const tsndgm::OrientationState &>));
            // ! END MODIFICATION
            BOOST_CONCEPT_ASSERT((DFSVisitorConcept<DGMVisitor, BidirectionalGraph>));
            typedef typename graph_traits<BidirectionalGraph>::vertex_descriptor Vertex;
            typedef typename graph_traits<BidirectionalGraph>::edge_descriptor Edge;
            BOOST_CONCEPT_ASSERT((ReadWritePropertyMapConcept<ColorMap, Vertex>));
            typedef typename property_traits<ColorMap>::value_type ColorValue;
            BOOST_CONCEPT_ASSERT((ColorValueConcept<ColorValue>));
            typedef color_traits<ColorValue> Color;
            // ! BEGIN MODIFICATION
            typedef typename tsndgm::NeighborVertexIterator<typename std::conditional<
                    reversed, std::map<Vertex, tsndgm::NeighborVertex>,
                    std::list<tsndgm::NeighborVertex>>::type>
                Iter;
            // ! END MODIFICATION
            typedef std::pair<Vertex,
                              std::pair<boost::optional<Edge>, std::pair<Iter, Iter>>>
                VertexInfo;

            boost::optional<Edge> src_e;
            Iter ei, ei_end;
            std::vector<VertexInfo> stack;

            // Possible optimization for vector
            // stack.reserve(num_vertices(g));

            // ! BEGIN MODIFICATION
            // Initialize DFS by pushing every vertex of u's group to the stack
            put(color, u, Color::gray());
            vis.discover_vertex(u, g);
            if constexpr (reversed)
            {
                if (g[u].neighbors_are_valid)
                    boost::tie(ei, ei_end) = tsndgm::restricted_in_edges(u, g).pair();
                else
                    boost::tie(ei, ei_end) = boost::in_edges(u, g);
            }
            else
            {
                if (g[u].neighbors_are_valid)
                    boost::tie(ei, ei_end) = tsndgm::restricted_out_edges(u, g).pair();
                else
                    boost::tie(ei, ei_end) = boost::out_edges(u, g);
            }
            if (func(u, g))
            {
                // If this vertex terminates the search, we push empty range
                stack.push_back(
                    std::make_pair(u, std::make_pair(boost::optional<Edge>(),
                                                     std::make_pair(ei_end, ei_end))));
            }
            else
            {
                stack.push_back(
                    std::make_pair(u, std::make_pair(boost::optional<Edge>(),
                                                     std::make_pair(ei, ei_end))));
            }
            // ! END MODIFICATION
            while (!stack.empty())
            {
                VertexInfo& back = stack.back();
                u = back.first;
                src_e = back.second.first;
                boost::tie(ei, ei_end) = back.second.second;
                stack.pop_back();
                // finish_edge has to be called here, not after the
                // loop. Think of the pop as the return from a recursive call.
                if (src_e)
                {
                    vis.finish_edge(*src_e, g);
                }
                while (ei != ei_end)
                {
                    // ! BEGIN MODIFICATION
                    if (g[(*ei).e].state() == tsndgm::blocked)
                    {
                        ++ei;
                        continue;
                    }

                    Vertex v = (*ei).v;
                    // ! END MODIFICATION
                    vis.examine_edge((*ei).e, g);
                    ColorValue v_color = get(color, v);
                    if (v_color == Color::white())
                    {
                        vis.tree_edge((*ei).e, g);
                        src_e = (*ei).e;
                        stack.push_back(std::make_pair(
                            u, std::make_pair(src_e, std::make_pair(++ei, ei_end))));
                        u = v;
                        put(color, u, Color::gray());
                        vis.discover_vertex(u, g);
                        // ! BEGIN MODIFICATION
                        if constexpr (reversed)
                        {
                            if (g[u].neighbors_are_valid)
                                boost::tie(ei, ei_end) = tsndgm::restricted_in_edges(u, g).pair();
                            else
                                boost::tie(ei, ei_end) = boost::in_edges(u, g);
                        }
                        else
                        {
                            if (g[u].neighbors_are_valid)
                                boost::tie(ei, ei_end) = tsndgm::restricted_out_edges(u, g).pair();
                            else
                                boost::tie(ei, ei_end) = boost::out_edges(u, g);
                        }
                        // ! END MODIFICATION
                        if (func(u, g))
                        {
                            ei = ei_end;
                        }
                    }
                    else
                    {
                        if (v_color == Color::gray())
                        {
                            if (vis.back_edge((*ei).e, g))
                                return;
                        }
                        else
                        {
                            vis.forward_or_cross_edge((*ei).e, g);
                        }
                        vis.finish_edge((*ei).e, g);
                        ++ei;
                    }
                }
                put(color, u, Color::black());
                vis.finish_vertex(u, g);
            }
        }
    } // namespace detail

    template <class VertexListGraph, class DFSVisitor, class ColorMap>
    void dgm_traversal(
        const VertexListGraph& g, DFSVisitor vis, ColorMap color,
        const bool reversed,
        typename graph_traits<VertexListGraph>::vertex_descriptor start_vertex)
    {
        typedef typename graph_traits<VertexListGraph>::vertex_descriptor Vertex;
        BOOST_CONCEPT_ASSERT((DFSVisitorConcept<DFSVisitor, VertexListGraph>));
        typedef typename property_traits<ColorMap>::value_type ColorValue;
        typedef color_traits<ColorValue> Color;

        typename graph_traits<VertexListGraph>::vertex_iterator ui, ui_end;
        for (boost::tie(ui, ui_end) = vertices(g); ui != ui_end; ++ui)
        {
            Vertex u = implicit_cast<Vertex>(*ui);
            put(color, u, Color::white());
            vis.initialize_vertex(u, g);
        }

        vis.reversed = reversed;
        vis.start_vertex(start_vertex, g);
        // ! BEGIN MODIFICATION
        if (reversed)
            detail::dgm_visit_impl<VertexListGraph, DFSVisitor, ColorMap,
                                   detail::nontruth2, true>(g, start_vertex, vis, color,
                                                            detail::nontruth2());
        else
            detail::dgm_visit_impl<VertexListGraph, DFSVisitor, ColorMap,
                                   detail::nontruth2, false>(
                g, start_vertex, vis, color, detail::nontruth2());
        // ! END MODIFICATION

        // for (boost::tie(ui, ui_end) = vertices(g); ui != ui_end; ++ui) {
        //   Vertex u = implicit_cast<Vertex>(*ui);
        //   ColorValue u_color = get(color, u);
        //   if (u_color == Color::white()) {
        //     vis.start_vertex(u, g);
        //     // ! BEGIN MODIFICATION
        //     if (reversed)
        //       detail::dgm_visit_impl<VertexListGraph, DFSVisitor, ColorMap,
        //                              detail::nontruth2, true>(g, u, vis, color,
        //                                                       detail::nontruth2());
        //     else
        //       detail::dgm_visit_impl<VertexListGraph, DFSVisitor, ColorMap,
        //                              detail::nontruth2, false>(g, u, vis, color,
        //                                                        detail::nontruth2());
        //     // ! END MODIFICATION
        //   }
        // }
    }

    template <class VertexListGraph, class DFSVisitor, class ColorMap>
    void dgm_traversal(const VertexListGraph& g, DFSVisitor vis, ColorMap color,
                       bool reversed)
    {
        typedef typename boost::graph_traits<VertexListGraph>::vertex_iterator vi;
        std::pair<vi, vi> verts = vertices(g);
        if (verts.first == verts.second)
            return;

        // ! BEGIN MODIFICATION
        dgm_traversal(g, vis, color, reversed,
                      detail::get_default_starting_vertex(g));
        // ! END MODIFICATION
    }

    // Boost.Parameter named parameter variant

    namespace graph
    {
        namespace detail
        {
            template <typename Graph, bool reversed>
            struct temp_dgm_traversal_impl
            {
                typedef void result_type;

                template <typename ArgPack>
                void operator()(const Graph& g, const ArgPack& arg_pack) const
                {
                    using namespace boost::graph::keywords;
                    boost::dgm_traversal(
                        g, arg_pack[_visitor | make_dfs_visitor(null_visitor())],
                        boost::detail::make_color_map_from_arg_pack(g, arg_pack), reversed,
                        arg_pack[_root_vertex ||
                            boost::detail::get_default_starting_vertex_t<Graph>(g)]);
                }
            };

            template <typename Graph>
            using dgm_traversal_impl = temp_dgm_traversal_impl<Graph, false>;

            template <typename Graph>
            using reversed_dgm_traversal_impl = temp_dgm_traversal_impl<Graph, true>;
        } // namespace detail
        BOOST_GRAPH_MAKE_FORWARDING_FUNCTION(dgm_traversal, 1, 4)
        BOOST_GRAPH_MAKE_FORWARDING_FUNCTION(reversed_dgm_traversal, 1, 4)
    } // namespace graph

    BOOST_GRAPH_MAKE_OLD_STYLE_PARAMETER_FUNCTION(dgm_traversal, 1)
    BOOST_GRAPH_MAKE_OLD_STYLE_PARAMETER_FUNCTION(reversed_dgm_traversal, 1)
} // namespace boost

#endif // TSN_DGM_TRAVERSAL_H
