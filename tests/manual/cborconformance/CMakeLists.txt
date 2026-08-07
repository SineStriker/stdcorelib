project(test_cborconformance)

file(GLOB _src *.h *.cpp)
add_executable(${PROJECT_NAME} ${_src})
target_link_libraries(${PROJECT_NAME} PRIVATE stdcorelib)
stdc_match_features(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
