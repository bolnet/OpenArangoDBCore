cmake_minimum_required(VERSION 3.20)

include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/v1.17.0.tar.gz
  URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
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

# --- Audit feature tests ---
add_executable(audit_feature_test
  unit/AuditFeatureTest.cpp
)
target_include_directories(audit_feature_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(audit_feature_test
  PRIVATE openarangodb_enterprise GTest::gtest_main GTest::gmock
)
gtest_discover_tests(audit_feature_test)

# --- LDAP handler tests ---
# Note: This test uses mock libldap (no real LDAP dependency).
# The LDAPHandler sources are compiled directly with ARANGODB_LDAP_MOCK_MODE
# so that the mock function pointers are used instead of real libldap.
add_executable(ldap_handler_test
  unit/LDAPHandlerTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Auth/LDAPHandler.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Auth/LDAPConfig.cpp
)
target_include_directories(ldap_handler_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_compile_definitions(ldap_handler_test PRIVATE ARANGODB_LDAP_MOCK_MODE=1)
target_link_libraries(ldap_handler_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(ldap_handler_test)

# --- Masking strategies tests ---
add_executable(masking_strategies_test
  unit/MaskingStrategiesTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Maskings/AttributeMasking.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/Maskings/AttributeMaskingEE.cpp
)
target_include_directories(masking_strategies_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(masking_strategies_test
  PRIVATE GTest::gtest_main GTest::gmock
)
gtest_discover_tests(masking_strategies_test)

# --- Encryption provider tests ---
# Compile encryption sources directly to avoid pulling in the full enterprise
# library (which requires ArangoDB headers not available in standalone builds).
add_executable(encryption_provider_test
  unit/EncryptionProviderTest.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/EncryptionProvider.cpp
  ${CMAKE_SOURCE_DIR}/Enterprise/RocksDBEngine/RocksDBEncryptionUtils.cpp
)
target_include_directories(encryption_provider_test
  BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
  PRIVATE
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(encryption_provider_test
  PRIVATE GTest::gtest_main GTest::gmock OpenSSL::Crypto
)
gtest_discover_tests(encryption_provider_test)
