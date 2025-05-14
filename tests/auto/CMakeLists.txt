project(tst_auto)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost CONFIG QUIET COMPONENTS unit_test_framework)

if(NOT Boost_FOUND)
    message(WARNING "Boost not found, auto tests will not be built")
    return()
endif()

file(GLOB _common_src *.cpp)
file(GLOB_RECURSE _subdir_src support/*.cpp)

if(WIN32)
    file(GLOB _platform_src platform/windows/*.cpp)
else()
    set(_platform_src)
endif()

set(_src ${_common_src} ${_subdir_src} ${_platform_src})
add_executable(${PROJECT_NAME} ${_src})

target_link_libraries(${PROJECT_NAME} PRIVATE Boost::unit_test_framework)

target_link_libraries(${PROJECT_NAME} PRIVATE stdcorelib)
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)