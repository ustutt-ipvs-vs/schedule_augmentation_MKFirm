include_directories(${CMAKE_SOURCE_DIR})

find_package(MPI REQUIRED)
add_library(communicator STATIC communicator.cc)
target_link_libraries(communicator PUBLIC MPI::MPI_CXX dgm)


add_library(optimization STATIC neighborhood.cc)
target_link_libraries(optimization PUBLIC dgm transform_heuristics communicator)
