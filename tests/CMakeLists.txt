add_executable(topology_test topology.cc)
target_link_libraries(topology_test network)

add_executable(route_test route.cc)
target_link_libraries(route_test network)

add_executable(message_stream_test message_stream.cc)
target_link_libraries(message_stream_test network)

add_test(NAME Routes COMMAND route_test)
