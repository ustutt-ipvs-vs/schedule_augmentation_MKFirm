cmake_minimum_required(VERSION 3.10)

project(libtsndgm)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(TESTING "1" CACHE STRING "Enable Testing")
set(BENCHMARKING "1" CACHE STRING "Enable Testing")

add_subdirectory(src)

add_library(tsndgm INTERFACE)
target_link_libraries(tsndgm INTERFACE optimization heuristics)

if (TESTING EQUAL "1") 
  enable_testing()
  add_subdirectory(tests)
endif()

if (BENCHMARKING EQUAL "1") 
  add_subdirectory(benchmarks)
endif()
