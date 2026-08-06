project(test_auto)

find_package(Boost CONFIG COMPONENTS unit_test_framework)

# Fatal rather than a warning and a return. Somebody who set STDC_BUILD_TESTS=ON and got a green
# build with no tests in it has been told nothing, which is how six CI jobs came back passing
# while running not one case.
if(NOT Boost_FOUND)
    message(FATAL_ERROR
        "STDC_BUILD_TESTS is on but Boost.Test was not found. Install it, or point CMake at it "
        "with -DCMAKE_PREFIX_PATH, or turn the tests off with -DSTDC_BUILD_TESTS=OFF.")
endif()

file(GLOB _common_src *.cpp)
file(GLOB_RECURSE _subdir_src
    support/*.cpp
    adt/*.cpp
    stlextra/*.cpp
)

if(WIN32)
    file(GLOB _platform_src platform/windows/*.cpp)
else()
    set(_platform_src)
endif()

set(_src ${_common_src} ${_subdir_src} ${_platform_src})
add_executable(${PROJECT_NAME} ${_src})

target_link_libraries(${PROJECT_NAME} PRIVATE Boost::unit_test_framework)

target_link_libraries(${PROJECT_NAME} PRIVATE stdcorelib)

# Some tests reach into private headers (src/*_p.h) to cover internals that the public API
# cannot get at on its own.
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# A module loaded at run time by the any tests. Whether a value keeps its identity across a
# module boundary cannot be answered from inside one binary, so this has to be a second one.
# Its visibility is hidden on purpose: that is the configuration in which identity schemes built
# on comparing addresses come apart.
add_library(test_plugin MODULE plugins/test_plugin.cpp)
target_link_libraries(test_plugin PRIVATE stdcorelib)
set_target_properties(test_plugin PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

add_dependencies(${PROJECT_NAME} test_plugin)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    TEST_PLUGIN_PATH="$<TARGET_FILE:test_plugin>"
)

# One entry rather than one per suite. The whole binary is a second, and twenty five process
# starts cost more than the suites they would be splitting.
#
# Note that ctest exits 0 when it finds no tests at all, so anything running this in anger wants
# --no-tests=error, or it is back to a green run of nothing.
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})