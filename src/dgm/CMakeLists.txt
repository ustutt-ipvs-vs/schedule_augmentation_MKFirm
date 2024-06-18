include_directories(${CMAKE_SOURCE_DIR})

file(GLOB_RECURSE SOURCE_FILES RELATIVE ${CMAKE_SOURCE_DIR}/src/dgm/ *.cc)
add_library(dgm STATIC ${SOURCE_FILES})
target_link_libraries(dgm network)
