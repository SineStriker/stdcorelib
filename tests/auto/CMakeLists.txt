project(test_auto)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost CONFIG QUIET COMPONENTS unit_test_framework)

if(NOT Boost_FOUND)
    message(WARNING "Boost not found, auto tests will not be built")
    return()
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
add_library(test_any_plugin MODULE plugins/any_plugin.cpp)
target_link_libraries(test_any_plugin PRIVATE stdcorelib)
set_target_properties(test_any_plugin PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

add_dependencies(${PROJECT_NAME} test_any_plugin)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    TEST_ANY_PLUGIN_PATH="$<TARGET_FILE:test_any_plugin>"
)