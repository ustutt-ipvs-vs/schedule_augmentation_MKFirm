include_directories(${CMAKE_SOURCE_DIR})

file(GLOB_RECURSE SOURCE_FILES RELATIVE ${CMAKE_SOURCE_DIR}/src/network/ *.cc)
add_library(network STATIC ${SOURCE_FILES})
target_link_libraries(network PUBLIC nlohmann_json::nlohmann_json)