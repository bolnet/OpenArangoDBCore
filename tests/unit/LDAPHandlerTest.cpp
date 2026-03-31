/// LDAP Handler unit tests.
/// Tests AUTH-01 through AUTH-05 requirements using mocked libldap.

#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <string>
#include <thread>
#include <vector>

// Mock mode is defined via CMake (ARANGODB_LDAP_MOCK_MODE=1)
#include "LDAPMocks.h"
#include "Enterprise/Auth/LDAPConfig.h"
#include "Enterprise/Auth/LDAPHandler.h"

namespace {

using namespace arangodb;

/// Helper to create an LDAPFunctions table pointing to mock implementations
LDAPFunctions makeMockFunctions() {
  LDAPFunctions funcs;
  funcs.initialize = mock_ldap_initialize;
  funcs.simpleBind = mock_ldap_simple_bind_s;
  funcs.searchExtS = mock_ldap_search_ext_s;
  funcs.setOption = mock_ldap_set_option;
  funcs.startTlsS = mock_ldap_start_tls_s;
  funcs.unbindExt = [](LDAP* ld, void* s, void* c) {
    return mock_ldap_unbind_ext(ld, s, c);
  };
  funcs.getDn = mock_ldap_get_dn;
  funcs.memfree = mock_ldap_memfree;
  funcs.firstEntry = mock_ldap_first_entry;
  funcs.getValuesLen = mock_ldap_get_values_len;
  funcs.valueFreeLen = mock_ldap_value_free_len;
  funcs.msgfree = mock_ldap_msgfree;
  return funcs;
}

// ============================================================
// Test Fixture
// ============================================================

class LDAPHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ldap_mock::state().reset();
  }

  void TearDown() override {
    // Verify no leaked handles
    EXPECT_EQ(ldap_mock::state().activeHandles.load(), 0)
        << "LDAP handle leak detected";
  }

  LDAPConfig makeSimpleConfig() {
    LDAPConfig cfg;
    cfg.server = "ldap.example.com";
    cfg.port = 389;
    cfg.basedn = "dc=example,dc=com";
    cfg.prefix = "uid=";
    cfg.suffix = ",ou=users,dc=example,dc=com";
    cfg.mode = LDAPAuthMode::Simple;
    return cfg;
  }

  LDAPConfig makeSearchConfig() {
    LDAPConfig cfg;
    cfg.server = "ldap.example.com";
    cfg.port = 389;
    cfg.basedn = "dc=example,dc=com";
    cfg.binddn = "cn=admin,dc=example,dc=com";
    cfg.bindpasswd = "adminpass";
    cfg.mode = LDAPAuthMode::Search;
    return cfg;
  }
};

// ============================================================
// LDAPConfig Tests
// ============================================================

TEST_F(LDAPHandlerTest, ConfigStoresAllOptions) {
  LDAPConfig cfg;
  cfg.server = "ldap.test.com";
  cfg.port = 636;
  cfg.basedn = "dc=test,dc=com";
  cfg.prefix = "cn=";
  cfg.suffix = ",ou=people,dc=test,dc=com";
  cfg.binddn = "cn=admin,dc=test,dc=com";
  cfg.bindpasswd = "secret";
  cfg.useTLS = false;
  cfg.useLDAPS = true;
  cfg.tlsCACertFile = "/etc/ssl/ca.pem";
  cfg.rolesAttribute = "memberOf";
  cfg.rolesSearch = "(member={userDn})";
  cfg.mode = LDAPAuthMode::Search;

  EXPECT_EQ(cfg.server, "ldap.test.com");
  EXPECT_EQ(cfg.port, 636);
  EXPECT_EQ(cfg.basedn, "dc=test,dc=com");
  EXPECT_EQ(cfg.prefix, "cn=");
  EXPECT_EQ(cfg.suffix, ",ou=people,dc=test,dc=com");
  EXPECT_EQ(cfg.binddn, "cn=admin,dc=test,dc=com");
  EXPECT_EQ(cfg.bindpasswd, "secret");
  EXPECT_FALSE(cfg.useTLS);
  EXPECT_TRUE(cfg.useLDAPS);
  EXPECT_EQ(cfg.tlsCACertFile, "/etc/ssl/ca.pem");
  EXPECT_EQ(cfg.rolesAttribute, "memberOf");
  EXPECT_EQ(cfg.rolesSearch, "(member={userDn})");
  EXPECT_EQ(cfg.mode, LDAPAuthMode::Search);
}

TEST_F(LDAPHandlerTest, ConfigValidationRejectsEmptyServer) {
  LDAPConfig cfg;
  cfg.server = "";
  auto err = cfg.validate();
  EXPECT_FALSE(err.empty());
  EXPECT_NE(err.find("server"), std::string::npos);
}

TEST_F(LDAPHandlerTest, ConfigValidationRejectsInvalidPort) {
  LDAPConfig cfg;
  cfg.server = "ldap.test.com";
  cfg.port = 0;
  EXPECT_FALSE(cfg.validate().empty());

  cfg.port = 70000;
  EXPECT_FALSE(cfg.validate().empty());
}

TEST_F(LDAPHandlerTest, ConfigValidationRejectsTLSAndLDAPS) {
  LDAPConfig cfg;
  cfg.server = "ldap.test.com";
  cfg.useTLS = true;
  cfg.useLDAPS = true;
  EXPECT_FALSE(cfg.validate().empty());
}

// ============================================================
// Simple Mode Authentication (AUTH-01)
// ============================================================

TEST_F(LDAPHandlerTest, SimpleModeConstrucsDNFromPrefixAndSuffix) {
  auto cfg = makeSimpleConfig();
  // Set up valid credentials for the constructed DN
  std::string expectedDn = "uid=testuser,ou=users,dc=example,dc=com";
  ldap_mock::state().validCredentials[expectedDn] = "testpass";

  LDAPHandler handler(cfg, makeMockFunctions());
  std::vector<std::string> roles;
  bool result = handler.authenticate("testuser", "testpass", roles);

  EXPECT_TRUE(result);

  // Verify the bind call used the constructed DN
  auto calls = ldap_mock::state().getCalls();
  bool foundBind = false;
  for (auto const& call : calls) {
    if (call.find("ldap_simple_bind_s:uid=testuser,ou=users,dc=example,dc=com") !=
        std::string::npos) {
      foundBind = true;
    }
  }
  EXPECT_TRUE(foundBind) << "Expected bind with constructed DN";
}

TEST_F(LDAPHandlerTest, SimpleModeInvalidCredentialsReturnsFalse) {
  auto cfg = makeSimpleConfig();
  ldap_mock::state().validCredentials["uid=testuser,ou=users,dc=example,dc=com"] =
      "correctpass";

  LDAPHandler handler(cfg, makeMockFunctions());
  std::vector<std::string> roles;
  bool result = handler.authenticate("testuser", "wrongpass", roles);

  EXPECT_FALSE(result);
  EXPECT_TRUE(roles.empty());
}

TEST_F(LDAPHandlerTest, SimpleModeValidCredentialsReturnsTrue) {
  auto cfg = makeSimpleConfig();
  std::string dn = "uid=alice,ou=users,dc=example,dc=com";
  ldap_mock::state().validCredentials[dn] = "alicepass";

  // Set up role attribute for alice
  cfg.rolesAttribute = "memberOf";
  MockLDAPMessage userEntry;
  userEntry.dn = dn;
  userEntry.attributes["memberOf"] = {"cn=admin,dc=example,dc=com",
                                       "cn=readers,dc=example,dc=com"};
  ldap_mock::state().searchResults.push_back(userEntry);

  LDAPHandler handler(cfg, makeMockFunctions());
  std::vector<std::string> roles;
  bool result = handler.authenticate("alice", "alicepass", roles);

  EXPECT_TRUE(result);
}

// ============================================================
// Search Mode Authentication (AUTH-01)
// ============================================================

TEST_F(LDAPHandlerTest, SearchModeUsesAdminBindThenSearches) {
  auto cfg = makeSearchConfig();
  // Admin credentials
  ldap_mock::state().validCredentials["cn=admin,dc=example,dc=com"] = "adminpass";

  // Search result: user DN found
  MockLDAPMessage searchResult;
  searchResult.dn = "uid=bob,ou=users,dc=example,dc=com";
  ldap_mock::state().searchResults.push_back(searchResult);

  // User credentials
  ldap_mock::state().validCredentials["uid=bob,ou=users,dc=example,dc=com"] = "bobpass";

  LDAPHandler handler(cfg, makeMockFunctions());
  std::vector<std::string> roles;
  bool result = handler.authenticate("bob", "bobpass", roles);

  EXPECT_TRUE(result);

  // Verify admin bind happened first, then search, then user bind
  auto calls = ldap_mock::state().getCalls();
  int adminBindIdx = -1, searchIdx = -1, userBindIdx = -1;
  for (int i = 0; i < static_cast<int>(calls.size()); ++i) {
    if (calls[i].find("ldap_simple_bind_s:cn=admin") != std::string::npos) {
      adminBindIdx = i;
    }
    if (calls[i].find("ldap_search_ext_s") != std::string::npos) {
      searchIdx = i;
    }
    if (calls[i].find("ldap_simple_bind_s:uid=bob") != std::string::npos) {
      userBindIdx = i;
    }
  }
  EXPECT_GT(adminBindIdx, -1) << "Admin bind not found";
  EXPECT_GT(searchIdx, adminBindIdx) << "Search should follow admin bind";
  EXPECT_GT(userBindIdx, searchIdx) << "User bind should follow search";
}

// ============================================================
// Per-Request Handle (AUTH-02)
// ============================================================

TEST_F(LDAPHandlerTest, CreateHandleReturnsFreshHandlePerCall) {
  auto cfg = makeSimpleConfig();
  std::string dn = "uid=user1,ou=users,dc=example,dc=com";
  ldap_mock::state().validCredentials[dn] = "pass1";

  LDAPHandler handler(cfg, makeMockFunctions());

  // Authenticate twice
  std::vector<std::string> roles1, roles2;
  handler.authenticate("user1", "pass1", roles1);

  ldap_mock::state().validCredentials[dn] = "pass1";
  handler.authenticate("user1", "pass1", roles2);

  // Each call should have created at least one handle
  auto calls = ldap_mock::state().getCalls();
  int initCount = 0;
  for (auto const& call : calls) {
    if (call.find("ldap_initialize") != std::string::npos) {
      initCount++;
    }
  }
  EXPECT_GE(initCount, 2) << "Expected at least 2 ldap_initialize calls";
}

TEST_F(LDAPHandlerTest, ConcurrentAuthenticateCallsNoInterference) {
  auto cfg = makeSimpleConfig();

  // Set up 8 different users
  for (int i = 0; i < 8; ++i) {
    std::string dn = "uid=user" + std::to_string(i) +
                     ",ou=users,dc=example,dc=com";
    ldap_mock::state().validCredentials[dn] = "pass" + std::to_string(i);
  }

  LDAPHandler handler(cfg, makeMockFunctions());

  std::atomic<int> successCount{0};
  std::vector<std::thread> threads;

  for (int i = 0; i < 8; ++i) {
    threads.emplace_back([&handler, &successCount, i]() {
      std::vector<std::string> roles;
      std::string user = "user" + std::to_string(i);
      std::string pass = "pass" + std::to_string(i);
      if (handler.authenticate(user, pass, roles)) {
        successCount.fetch_add(1);
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(successCount.load(), 8)
      << "All 8 concurrent authenticate calls should succeed";

  // All handles should be cleaned up
  EXPECT_EQ(ldap_mock::state().activeHandles.load(), 0)
      << "All handles should be destroyed after authentication";
}

// ============================================================
// TLS Support (AUTH-05)
// ============================================================

TEST_F(LDAPHandlerTest, TLSModeCallsStartTLS) {
  auto cfg = makeSimpleConfig();
  cfg.useTLS = true;
  std::string dn = "uid=tlsuser,ou=users,dc=example,dc=com";
  ldap_mock::state().validCredentials[dn] = "tlspass";

  LDAPHandler handler(cfg, makeMockFunctions());
  std::vector<std::string> roles;
  handler.authenticate("tlsuser", "tlspass", roles);

  auto calls = ldap_mock::state().getCalls();
  bool foundStartTls = false;
  for (auto const& call : calls) {
    if (call.find("ldap_start_tls_s") != std::string::npos) {
      foundStartTls = true;
    }
  }
  EXPECT_TRUE(foundStartTls) << "StartTLS should be called when useTLS=true";
}

TEST_F(LDAPHandlerTest, LDAPSModeUsesLdapsURI) {
  auto cfg = makeSimpleConfig();
  cfg.useLDAPS = true;
  cfg.port = 636;
  std::string dn = "uid=ssluser,ou=users,dc=example,dc=com";
  ldap_mock::state().validCredentials[dn] = "sslpass";

  LDAPHandler handler(cfg, makeMockFunctions());
  std::vector<std::string> roles;
  handler.authenticate("ssluser", "sslpass", roles);

  auto calls = ldap_mock::state().getCalls();
  bool foundLdaps = false;
  for (auto const& call : calls) {
    if (call.find("ldap_initialize:ldaps://") != std::string::npos) {
      foundLdaps = true;
    }
  }
  EXPECT_TRUE(foundLdaps) << "LDAPS mode should use ldaps:// URI";
}

TEST_F(LDAPHandlerTest, TLSCACertFileSetOption) {
  auto cfg = makeSimpleConfig();
  cfg.useTLS = true;
  cfg.tlsCACertFile = "/etc/ssl/ca-cert.pem";
  std::string dn = "uid=certuser,ou=users,dc=example,dc=com";
  ldap_mock::state().validCredentials[dn] = "certpass";

  LDAPHandler handler(cfg, makeMockFunctions());
  std::vector<std::string> roles;
  handler.authenticate("certuser", "certpass", roles);

  auto calls = ldap_mock::state().getCalls();
  bool foundCACert = false;
  for (auto const& call : calls) {
    if (call.find("ldap_set_option:" + std::to_string(LDAP_OPT_X_TLS_CACERTFILE)) !=
        std::string::npos) {
      foundCACert = true;
    }
  }
  EXPECT_TRUE(foundCACert) << "CA cert file should be set via ldap_set_option";
}

// ============================================================
// Role Mapping via Attribute (AUTH-03)
// ============================================================

TEST_F(LDAPHandlerTest, FetchRolesViaAttributeReturnsRoles) {
  auto cfg = makeSimpleConfig();
  cfg.rolesAttribute = "memberOf";
  std::string dn = "uid=roleuser,ou=users,dc=example,dc=com";
  ldap_mock::state().validCredentials[dn] = "rolepass";

  MockLDAPMessage userEntry;
  userEntry.dn = dn;
  userEntry.attributes["memberOf"] = {"cn=admins,dc=example,dc=com",
                                       "cn=developers,dc=example,dc=com"};
  ldap_mock::state().searchResults.push_back(userEntry);

  LDAPHandler handler(cfg, makeMockFunctions());
  std::vector<std::string> roles;
  bool result = handler.authenticate("roleuser", "rolepass", roles);

  EXPECT_TRUE(result);
  EXPECT_EQ(roles.size(), 2u);
  EXPECT_TRUE(std::find(roles.begin(), roles.end(),
                        "cn=admins,dc=example,dc=com") != roles.end());
  EXPECT_TRUE(std::find(roles.begin(), roles.end(),
                        "cn=developers,dc=example,dc=com") != roles.end());
}

// ============================================================
// Role Mapping via Search (AUTH-03)
// ============================================================

TEST_F(LDAPHandlerTest, FetchRolesViaSearchReturnsGroups) {
  auto cfg = makeSimpleConfig();
  cfg.rolesSearch = "(member={userDn})";
  std::string dn = "uid=searchroleuser,ou=users,dc=example,dc=com";
  ldap_mock::state().validCredentials[dn] = "searchrolepass";

  // First search returns the user (for bind success in simple mode this is
  // not needed, but for role search the mock returns groups)
  MockLDAPMessage groupEntry;
  groupEntry.dn = "cn=editors,ou=groups,dc=example,dc=com";
  groupEntry.attributes["cn"] = {"editors"};
  ldap_mock::state().searchResults.push_back(groupEntry);

  LDAPHandler handler(cfg, makeMockFunctions());
  std::vector<std::string> roles;
  bool result = handler.authenticate("searchroleuser", "searchrolepass", roles);

  EXPECT_TRUE(result);
  // Should have at least the group DN as a role
  EXPECT_FALSE(roles.empty());
}

// ============================================================
// Mapped Roles are Valid ArangoDB Role Names (AUTH-04)
// ============================================================

TEST_F(LDAPHandlerTest, MappedRolesAreValidArangoDBRoleNames) {
  auto cfg = makeSimpleConfig();
  cfg.rolesAttribute = "memberOf";
  std::string dn = "uid=validroles,ou=users,dc=example,dc=com";
  ldap_mock::state().validCredentials[dn] = "validpass";

  MockLDAPMessage userEntry;
  userEntry.dn = dn;
  // Roles should be non-empty strings
  userEntry.attributes["memberOf"] = {"cn=rw,dc=example,dc=com",
                                       "cn=ro,dc=example,dc=com"};
  ldap_mock::state().searchResults.push_back(userEntry);

  LDAPHandler handler(cfg, makeMockFunctions());
  std::vector<std::string> roles;
  handler.authenticate("validroles", "validpass", roles);

  for (auto const& role : roles) {
    EXPECT_FALSE(role.empty()) << "Role names must not be empty";
  }
}

}  // namespace
