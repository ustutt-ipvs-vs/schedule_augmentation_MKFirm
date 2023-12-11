include_directories(${CMAKE_SOURCE_DIR})

add_library(optimization STATIC neighborhood.cc)
target_link_libraries(optimization dgm transform_heuristics)
