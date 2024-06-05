include_directories(${CMAKE_SOURCE_DIR})

add_library(network STATIC topology.cc route.cc message_stream.cc stream_schedule.cc frame_schedule.cc)
target_link_libraries(network PUBLIC nlohmann_json::nlohmann_json)