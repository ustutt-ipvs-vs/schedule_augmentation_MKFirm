include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG f8d7d77c06936315286eb55f8de22cd23c188571
)
FetchContent_MakeAvailable(googletest)

add_executable(boost_graph boost_graph.cc)

add_executable(topology_test topology.cc)
target_link_libraries(topology_test network)

add_executable(route_test route.cc)
target_link_libraries(route_test network)

add_test(NAME Routes COMMAND route_test)

add_executable(message_stream_test message_stream.cc)
target_link_libraries(message_stream_test network)

add_executable(traversal_test traversal.cc)
target_link_libraries(traversal_test dgm)

add_executable(dgm_test dgm.cc)
target_link_libraries(dgm_test dgm GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(dgm_test)
gtest_discover_tests(tabu_search_test)
