cmake_minimum_required(VERSION 3.20)

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/v1.17.0.tar.gz
  URL_HASH SHA256=65fab70b01e38b24b55e1c5c65e15a674d3b2f3b6f7b7b5e5a9d8e7c7e6a8f2d
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

# --- Unit tests ---
add_executable(license_feature_test
  unit/LicenseFeatureTest.cpp
)
target_include_directories(license_feature_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(license_feature_test
  PRIVATE openarangodb_enterprise GTest::gtest_main GTest::gmock
)
gtest_discover_tests(license_feature_test)

# --- Link symbol tests ---
add_executable(link_symbol_test
  link/test_link_symbols.cpp
)
target_include_directories(link_symbol_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(link_symbol_test
  PRIVATE openarangodb_enterprise GTest::gtest_main
)
gtest_discover_tests(link_symbol_test)
