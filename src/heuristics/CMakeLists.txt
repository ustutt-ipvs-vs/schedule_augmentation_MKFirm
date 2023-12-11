include_directories(${CMAKE_SOURCE_DIR})

add_library(initial_heuristics STATIC initial.cc)
target_link_libraries(initial_heuristics dgm)

add_library(transform_heuristics STATIC transform.cc)
target_link_libraries(transform_heuristics dgm optimization)
