cmake_minimum_required(VERSION 3.10)

project(libtsndgm)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(TESTING "1" CACHE STRING "Enable Testing")
set(BENCHMARKING "1" CACHE STRING "Enable Testing")


## dependencies
# download JSPLIB, which contains Job-Shop Scheduling Problems
find_package(Git QUIET)
if (GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    # Update submodules as needed
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if (GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive ${PROJECT_SOURCE_DIR}/data/JSPLIB
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if (NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif ()
    endif ()
endif ()
if (NOT EXISTS "${PROJECT_SOURCE_DIR}/data/JSPLIB/instances.json")
    message(FATAL_ERROR "The submodules were not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif ()

include(FetchContent)
FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

## tsndgm library
add_subdirectory(src)

add_library(tsndgm INTERFACE)
target_link_libraries(tsndgm INTERFACE optimization heuristics)

## Executable
add_executable(DgmExec main.cpp)

target_link_libraries(DgmExec
        tsndgm
)


if (TESTING EQUAL "1")
    enable_testing()
    add_subdirectory(tests)
endif ()
