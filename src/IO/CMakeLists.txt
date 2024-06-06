include_directories(${CMAKE_SOURCE_DIR})

add_library(io STATIC programOptions.cc schedule_loader.cc)
target_link_libraries(io PUBLIC CLI11 nlohmann_json::nlohmann_json)