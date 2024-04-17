include_directories(${CMAKE_SOURCE_DIR})

add_library(initial_heuristics STATIC initial.cc)
target_link_libraries(initial_heuristics PUBLIC dgm)

add_library(heuristics INTERFACE)
target_link_libraries(heuristics INTERFACE initial_heuristics)
