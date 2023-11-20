#include "../src/dgm/traversal.h"
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/pending/indirect_cmp.hpp>
#include <boost/range/irange.hpp>
#include <iostream>

using namespace boost;

template <typename TimeMap>
class dfs_time_visitor : public default_dfs_visitor {
  typedef typename property_traits<TimeMap>::value_type T;

public:
  dfs_time_visitor(TimeMap dmap, TimeMap fmap, T &t)
      : m_dtimemap(dmap), m_ftimemap(fmap), m_time(t) {}

  template <typename Vertex, typename Graph>
  void discover_vertex(Vertex u, const Graph &g) const {
    put(m_dtimemap, u, m_time++);
  }

  template <typename Vertex, typename Graph>
  void finish_vertex(Vertex u, const Graph &g) const {
    put(m_ftimemap, u, m_time++);
  }

  template <typename Edge, typename Graph>
  void back_edge(Edge v, const Graph &g) const {
    std::cout << "Back edge: " << source(v, g) << " -> " << target(v, g)
              << std::endl;
  }

  TimeMap m_dtimemap;
  TimeMap m_ftimemap;
  T &m_time;
};

int main() {
  struct VertexProperty;

  struct EdgeProperty {
    OrientationState _state;

    const boost::OrientationState &state() const { return _state; }
  };

  typedef adjacency_list<vecS, vecS, bidirectionalS, VertexProperty,
                         EdgeProperty>
      graph_t;

  typedef graph_traits<graph_t>::vertex_descriptor Vertex;
  typedef graph_traits<graph_t>::vertices_size_type size_type;

  struct VertexProperty {
    std::shared_ptr<std::list<Vertex>> group;
  };

  std::string name[] = {
      "([T_1,B_0], f_1)", "([B_0,B_2], f_1)", "([B_2,L_1], f_1)",
      "([T_1,B_1], f_2)", "([B_1,B_2], f_2)", "([B_2,L_1], f_2)",
      "([T_1,B_1], f_3)", "([B_1,B_2], f_3)", "([B_2,L_1], f_3)",
  };
  unsigned int N = 9;

  graph_t g(N);

  for (unsigned int i = 0; i < 9; i++) {
    g[i].group =
        std::make_shared<std::list<Vertex>>(std::initializer_list<Vertex>{i});
  }

  add_edge(0, 1, EdgeProperty(allowed), g);
  add_edge(1, 2, EdgeProperty(allowed), g);
  add_edge(3, 4, EdgeProperty(allowed), g);
  add_edge(4, 5, EdgeProperty(allowed), g);
  add_edge(6, 7, EdgeProperty(allowed), g);
  add_edge(7, 8, EdgeProperty(allowed), g);

  add_edge(3, 6, EdgeProperty(allowed), g);
  add_edge(6, 3, EdgeProperty(blocked), g);
  add_edge(4, 7, EdgeProperty(allowed), g);
  add_edge(7, 4, EdgeProperty(blocked), g);
  add_edge(5, 8, EdgeProperty(allowed), g);
  add_edge(8, 5, EdgeProperty(blocked), g);
  add_edge(5, 2, EdgeProperty(blocked), g);
  add_edge(2, 5, EdgeProperty(allowed), g);
  add_edge(2, 8, EdgeProperty(blocked), g);
  add_edge(8, 2, EdgeProperty(allowed), g);

  std::vector<size_type> dtime(num_vertices(g));
  std::vector<size_type> ftime(num_vertices(g));
  typedef iterator_property_map<
      std::vector<size_type>::iterator,
      property_map<graph_t, vertex_index_t>::const_type>
      time_pm_type;
  time_pm_type dtime_pm(dtime.begin(), get(vertex_index, g));
  time_pm_type ftime_pm(ftime.begin(), get(vertex_index, g));
  size_type t = 0;
  dfs_time_visitor<time_pm_type> vis(dtime_pm, ftime_pm, t);

  dgm_traversal(g, visitor(vis).root_vertex(3));

  // use std::sort to order the vertices by their discover time
  std::vector<size_type> discover_order(N);
  integer_range<size_type> r(0, N);
  std::copy(r.begin(), r.end(), discover_order.begin());
  std::sort(discover_order.begin(), discover_order.end(),
            indirect_cmp<time_pm_type, std::less<size_type>>(dtime_pm));
  std::cout << "order of discovery: ";
  int i;
  for (i = 0; i < N; ++i)
    std::cout << name[discover_order[i]] << " ";

  std::vector<size_type> finish_order(N);
  std::copy(r.begin(), r.end(), finish_order.begin());
  std::sort(finish_order.begin(), finish_order.end(),
            indirect_cmp<time_pm_type, std::less<size_type>>(ftime_pm));
  std::cout << std::endl << "order of finish: ";
  for (i = 0; i < N; ++i)
    std::cout << name[finish_order[i]] << " ";
  std::cout << std::endl;

  return 0;
}
