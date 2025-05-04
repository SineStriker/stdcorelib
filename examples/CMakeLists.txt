file(GLOB _common_src *.cpp)
file(GLOB_RECURSE _subdir_src support/*.cpp)

if(WIN32)
    file(GLOB _platform_src platform/windows/*.cpp)
else()
    set(_platform_src)
endif()

set(_src ${_common_src} ${_subdir_src} ${_platform_src})

foreach(_file IN LISTS _src)
    get_filename_component(_name ${_file} NAME_WE)
    set(_project_name "example_${_name}")
    add_executable(${_project_name} ${_file})
    target_link_libraries(${_project_name} PRIVATE stdcorelib)
endforeach()