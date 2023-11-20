cmake_minimum_required(VERSION 3.10)

project(libtsndgm)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#set(CMAKE_CXX_CLANG_TIDY
#    clang-tidy;
#    -checks=clang-analyzer-*,boost-*,cert-*,google-*,performance-*               # Specify the checks you want
#    -header-filter=.*       # Filter headers
#)

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)
