cmake_minimum_required(VERSION 3.10)

project(libtsndgm)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include(FetchContent)
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
        CLI11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.4.2
)
FetchContent_MakeAvailable(CLI11)


## tsndgm library
add_subdirectory(src)

## Executable
add_executable(DgmExec main.cpp)
target_link_libraries(DgmExec
        dgm
        io
)
