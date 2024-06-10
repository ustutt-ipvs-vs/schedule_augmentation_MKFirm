include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG f8d7d77c06936315286eb55f8de22cd23c188571
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

file(GLOB_RECURSE SOURCE_FILES RELATIVE ${CMAKE_SOURCE_DIR}/tests/ *.cpp)
add_executable(unit_tests ${SOURCE_FILES})

target_link_libraries(unit_tests LINK_PUBLIC
        gtest
        dgm
        io
)

target_include_directories(
        unit_tests PUBLIC
        gtest
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_test(
        NAME unit_tests
        COMMAND
        ${CMAKE_BINARY_DIR}/test/unit_tests
)
