add_executable(generate_fixtures main.cpp)
target_link_libraries(generate_fixtures PRIVATE Microsoft::DirectXTex)
target_compile_features(generate_fixtures PRIVATE cxx_std_23)
set_target_properties(generate_fixtures PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
