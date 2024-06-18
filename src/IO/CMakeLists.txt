include_directories(${CMAKE_SOURCE_DIR})

file(GLOB_RECURSE SOURCE_FILES RELATIVE ${CMAKE_SOURCE_DIR}/src/IO/ *.cc)
add_library(io STATIC ${SOURCE_FILES})
target_link_libraries(io PUBLIC
        CLI11
        nlohmann_json::nlohmann_json
        network
)
