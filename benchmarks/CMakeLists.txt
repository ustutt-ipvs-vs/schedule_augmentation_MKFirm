# download JSPLIB, which contains Job-Shop Scheduling Problems
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
# Update submodules as needed
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
	execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive ${PROJECT_SOURCE_DIR}/data/JSPLIB
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()
if(NOT EXISTS "${PROJECT_SOURCE_DIR}/data/JSPLIB/instances.json")
    message(FATAL_ERROR "The submodules were not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif()

# JSPLIB contains metadata in instances.json
# we use nlohmann_json to read it
find_package(nlohmann_json 3.2.0 REQUIRED)

add_executable(jsp jsp.cc)
target_link_libraries(jsp PUBLIC tsndgm)

add_executable(baseline_scheduling baseline_scheduling.cc)
target_link_libraries(baseline_scheduling PUBLIC tsndgm)

add_executable(graceful_degradation_cyclic graceful_degradation_cyclic.cc)
target_link_libraries(graceful_degradation_cyclic PUBLIC tsndgm)

add_executable(graceful_degradation_isochronous graceful_degradation_isochronous.cc)
target_link_libraries(graceful_degradation_isochronous PUBLIC tsndgm)

add_executable(channel_improvement_cyclic channel_improvement_cyclic.cc)
target_link_libraries(channel_improvement_cyclic PUBLIC tsndgm)

add_executable(pdc pdc.cc)
target_link_libraries(pdc PUBLIC tsndgm)

add_executable(reliability reliability.cc)
target_link_libraries(reliability PUBLIC tsndgm)
