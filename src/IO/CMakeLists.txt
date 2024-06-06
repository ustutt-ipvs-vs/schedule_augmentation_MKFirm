include_directories(${CMAKE_SOURCE_DIR})

add_library(io STATIC programOptions.cc inputLoader.cc scheduleLoader.cc)
target_link_libraries(io PUBLIC
        CLI11
        nlohmann_json::nlohmann_json
        network
)
