include_directories(${CMAKE_SOURCE_DIR})

add_library(dgm STATIC dgm.cc critical_path.cc)
target_link_libraries(dgm network)
