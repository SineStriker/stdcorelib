cmake_minimum_required(VERSION 3.16)

project(cpputils VERSION 0.0.1.1 LANGUAGES CXX)

# ----------------------------------
# Build Options
# ----------------------------------
if(NOT DEFINED CPPUTILS_BUILD_STATIC)
    set(CPPUTILS_BUILD_STATIC on)
endif()

if(NOT DEFINED CPPUTILS_BUILD_TESTS)
    set(CPPUTILS_BUILD_TESTS off)
endif()

if(NOT DEFINED CPPUTILS_INSTALL)
    set(CPPUTILS_INSTALL on)
endif()

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

if(CPPUTILS_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
endif()

# ----------------------------------
# Project Variables
# ----------------------------------
set(CPPUTILS_VERSION ${PROJECT_VERSION})
set(CPPUTILS_INSTALL_NAME ${PROJECT_NAME})

# ----------------------------------
# Main Project
# ----------------------------------
file(GLOB_RECURSE _src include/*.h src/*.h src/*.cpp)

if(CPPUTILS_BUILD_STATIC)
    add_library(${PROJECT_NAME} STATIC)
    target_compile_definitions(${PROJECT_NAME} PUBLIC CPPUTILS_STATIC)
else()
    add_library(${PROJECT_NAME} SHARED)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE CPPUTILS_LIBRARY)
target_sources(${PROJECT_NAME} PRIVATE ${_src})
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_include_directories(${PROJECT_NAME} PRIVATE src include/cpputils)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

# Add platform specific
if(WIN32)
    set(RC_DESCRIPTION "C++ Utility Library")
    set(RC_COPYRIGHT "Copyright (C) 2022-2024 SineStriker")
    include("cmake/winrc.cmake")
endif()

if(CPPUTILS_INSTALL)
    install(TARGETS ${PROJECT_NAME}
        EXPORT ${CPPUTILS_INSTALL_NAME}Targets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" OPTIONAL
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" OPTIONAL
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" OPTIONAL
    )
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE shlwapi)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE dl)
endif()

if(CPPUTILS_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(CPPUTILS_INSTALL)
    # Add install target
    set(_install_dir ${CMAKE_INSTALL_LIBDIR}/cmake/${CPPUTILS_INSTALL_NAME})

    # Add version file
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${CPPUTILS_INSTALL_NAME}ConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
    )

    # Add configuration file
    configure_package_config_file(
        ${CMAKE_CURRENT_LIST_DIR}/${CPPUTILS_INSTALL_NAME}Config.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/${CPPUTILS_INSTALL_NAME}Config.cmake"
        INSTALL_DESTINATION ${_install_dir}
        NO_CHECK_REQUIRED_COMPONENTS_MACRO
    )

    # Install cmake files
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${CPPUTILS_INSTALL_NAME}Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${CPPUTILS_INSTALL_NAME}ConfigVersion.cmake"
        DESTINATION ${_install_dir}
    )

    # Install cmake targets files
    install(EXPORT ${CPPUTILS_INSTALL_NAME}Targets
        FILE "${CPPUTILS_INSTALL_NAME}Targets.cmake"
        NAMESPACE cpputils::
        DESTINATION ${_install_dir}
    )

    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )
endif()