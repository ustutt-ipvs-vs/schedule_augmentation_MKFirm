include_directories(${CMAKE_SOURCE_DIR})

add_library(dgm STATIC dgm.cc critical_path.cc tsn_configuration.cc graph_visitors.cc)
target_link_libraries(dgm network)
