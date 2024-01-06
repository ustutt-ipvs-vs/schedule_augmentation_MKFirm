cmake_minimum_required(VERSION 3.10)

project(libtsndgm)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(TESTING "1" CACHE STRING "Enable Testing")
set(BENCHMARKING "1" CACHE STRING "Enable Testing")

#set(CMAKE_CXX_CLANG_TIDY
#    clang-tidy;
#    -checks=clang-analyzer-*,boost-*,cert-*,google-*,performance-*               # Specify the checks you want
#    -header-filter=.*       # Filter headers
#)

add_subdirectory(src)

if (TESTING EQUAL "1") 
  enable_testing()
  add_subdirectory(tests)
endif()

if (BENCHMARKING EQUAL "1") 
  add_subdirectory(benchmarks)
endif()
