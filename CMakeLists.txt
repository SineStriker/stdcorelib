cmake_minimum_required(VERSION 3.16)

project(stdcorelib VERSION 0.0.1.1 LANGUAGES CXX)

# ----------------------------------
# Build Options
# ----------------------------------
option(STDCORELIB_BUILD_STATIC "Build static library" ON)
option(STDCORELIB_BUILD_TESTS "Build test cases" OFF)
option(STDCORELIB_INSTALL "Install library" ON)

# ----------------------------------
# CMake Settings
# ----------------------------------
if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
endif()

if(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
endif()

if(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
endif()

if(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /manifest:no")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} /manifest:no")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /manifest:no")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")

    if(NOT DEFINED CMAKE_DEBUG_POSTFIX)
        set(CMAKE_DEBUG_POSTFIX "d")
    endif()
elseif(MINGW)
    set(CMAKE_STATIC_LIBRARY_PREFIX "")
    set(CMAKE_SHARED_LIBRARY_PREFIX "")
endif()

if(STDCORELIB_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
endif()

# ----------------------------------
# Project Variables
# ----------------------------------
set(STDCORELIB_VERSION ${PROJECT_VERSION})
set(STDCORELIB_INSTALL_NAME ${PROJECT_NAME})

# ----------------------------------
# Main Project
# ----------------------------------
file(GLOB_RECURSE _src include/*.h src/*.h src/*.cpp)

if(NOT WIN32)
    set(_temp)

    foreach(_item IN LISTS _src)
        get_filename_component(_name ${_item} NAME)

        if(_name MATCHES "^windows_.+")
            continue()
        endif()

        list(APPEND _temp ${_item})
    endforeach()

    set(_src ${_temp})
    unset(_temp)
endif()

if(STDCORELIB_BUILD_STATIC)
    add_library(${PROJECT_NAME} STATIC)
    target_compile_definitions(${PROJECT_NAME} PUBLIC STDCORELIB_STATIC)
else()
    add_library(${PROJECT_NAME} SHARED)
endif()

target_sources(${PROJECT_NAME} PRIVATE ${_src})
target_compile_definitions(${PROJECT_NAME} PRIVATE STDCORELIB_LIBRARY)
target_include_directories(${PROJECT_NAME} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
if (MSVC)
    target_include_directories(${PROJECT_NAME} PRIVATE include/stdcorelib)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -idirafter ${CMAKE_CURRENT_SOURCE_DIR}/include/stdcorelib)
endif()
target_include_directories(${PROJECT_NAME} PRIVATE
    src
    include
    include/stdcorelib/private
)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# Add platform specific
if(WIN32)
    set(RC_DESCRIPTION "C++ Additional Core Library")
    set(RC_COPYRIGHT "Copyright (C) 2022-2024 SineStriker")
    include("cmake/winrc.cmake")
endif()

if(STDCORELIB_INSTALL)
    install(TARGETS ${PROJECT_NAME}
        EXPORT ${STDCORELIB_INSTALL_NAME}Targets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" OPTIONAL
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" OPTIONAL
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" OPTIONAL
    )
endif()

if(NOT WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE dl)
endif()

if(STDCORELIB_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(STDCORELIB_INSTALL)
    # Add install target
    set(_install_dir ${CMAKE_INSTALL_LIBDIR}/cmake/${STDCORELIB_INSTALL_NAME})

    # Add version file
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${STDCORELIB_INSTALL_NAME}ConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
    )

    # Add configuration file
    configure_package_config_file(
        ${CMAKE_CURRENT_LIST_DIR}/${STDCORELIB_INSTALL_NAME}Config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/${STDCORELIB_INSTALL_NAME}Config.cmake"
        INSTALL_DESTINATION ${_install_dir}
        NO_CHECK_REQUIRED_COMPONENTS_MACRO
    )

    # Install cmake files
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${STDCORELIB_INSTALL_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${STDCORELIB_INSTALL_NAME}ConfigVersion.cmake"
        DESTINATION ${_install_dir}
    )

    # Install cmake targets files
    install(EXPORT ${STDCORELIB_INSTALL_NAME}Targets
        FILE "${STDCORELIB_INSTALL_NAME}Targets.cmake"
        NAMESPACE stdcorelib::
        DESTINATION ${_install_dir}
    )

    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )
endif()