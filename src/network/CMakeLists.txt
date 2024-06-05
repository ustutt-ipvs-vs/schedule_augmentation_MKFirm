include_directories(${CMAKE_SOURCE_DIR})

add_library(network STATIC topology.cc message_stream.cc)
target_link_libraries(network PUBLIC nlohmann_json::nlohmann_json)