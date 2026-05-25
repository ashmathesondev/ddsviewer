find_package(GTest CONFIG REQUIRED)

add_subdirectory(generate_fixtures)

add_executable(ddsviewer_tests
    test_dds_loader.cpp
    test_texture_view.cpp
    ${CMAKE_SOURCE_DIR}/src/DDSLoader.cpp
)

target_include_directories(ddsviewer_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_definitions(ddsviewer_tests PRIVATE
    FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

target_link_libraries(ddsviewer_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    Microsoft::DirectXTex
)

target_compile_features(ddsviewer_tests PRIVATE cxx_std_23)

include(GoogleTest)
gtest_discover_tests(ddsviewer_tests)
