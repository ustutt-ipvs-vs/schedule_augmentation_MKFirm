include_directories(${CMAKE_SOURCE_DIR})

find_package(MPI REQUIRED)
add_library(communicator STATIC communicator.cc)
target_link_libraries(communicator PUBLIC MPI::MPI_CXX dgm)

add_library(neighborhood STATIC neighborhood.cc)
target_link_libraries(neighborhood PUBLIC dgm)

add_library(selection_storage STATIC selection_storage.cc)
target_link_libraries(selection_storage PUBLIC dgm)

add_library(optimization INTERFACE)
target_link_libraries(optimization INTERFACE communicator neighborhood selection_storage)
