include_directories(${CMAKE_SOURCE_DIR})

add_library(io STATIC programOptions.cc)
target_link_libraries(io PUBLIC CLI11)