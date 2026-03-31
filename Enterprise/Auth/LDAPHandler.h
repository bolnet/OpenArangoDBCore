#pragma once
#ifndef ARANGODB_LDAP_HANDLER_H
#define ARANGODB_LDAP_HANDLER_H

#include "Enterprise/Auth/LDAPConfig.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// Forward declarations for LDAP types.
// When building with real libldap, include <ldap.h>.
// When building tests, the mock header provides these types.
// ---------------------------------------------------------------------------
#ifdef ARANGODB_LDAP_MOCK_MODE
#include "LDAPMocks.h"
#else
#ifdef ARANGODB_HAVE_LDAP
#include <ldap.h>
#else
// Stub mode: no LDAP available
struct MockLDAPHandle { int id = 0; bool bound = false; std::string boundDn; };
struct MockLDAPMessage {
  std::string dn;
  std::unordered_map<std::string, std::vector<std::string>> attributes;
};
struct MockBerval { char* bv_val; int bv_len; };
using LDAP = MockLDAPHandle;
using LDAPMessage = MockLDAPMessage;
using BerValue = MockBerval;
#define LDAP_SUCCESS 0
#define LDAP_INVALID_CREDENTIALS 49
#define LDAP_NO_SUCH_OBJECT 32
#define LDAP_SCOPE_SUBTREE 0x0002
#define LDAP_OPT_X_TLS 0x6000
#define LDAP_OPT_X_TLS_CACERTFILE 0x6002
#define LDAP_OPT_ON ((void*)1)
#endif
#endif

namespace arangodb {

/// Function pointer table for libldap operations.
/// In production, these point to real libldap functions.
/// In tests, these are replaced by mock implementations.
struct LDAPFunctions {
  std::function<int(LDAP**, char const*)> initialize;
  std::function<int(LDAP*, char const*, char const*)> simpleBind;
  std::function<int(LDAP*, char const*, int, char const*, char**,
                    int, void*, void*, void*, int, LDAPMessage**)> searchExtS;
  std::function<int(LDAP*, int, void const*)> setOption;
  std::function<int(LDAP*, void*, void*)> startTlsS;
  std::function<int(LDAP*, void*, void*)> unbindExt;
  std::function<char*(LDAP*, LDAPMessage*)> getDn;
  std::function<void(void*)> memfree;
  std::function<LDAPMessage*(LDAP*, LDAPMessage*)> firstEntry;
  std::function<BerValue**(LDAP*, LDAPMessage*, char const*)> getValuesLen;
  std::function<void(BerValue**)> valueFreeLen;
  std::function<void(LDAPMessage*)> msgfree;
};

/// Thread-safe LDAP authentication handler.
///
/// CRITICAL DESIGN: No shared LDAP* member. Each authenticate() call creates
/// and destroys its own LDAP handle via createHandle()/destroyHandle().
/// This ensures thread safety under concurrent access (AUTH-02).
class LDAPHandler {
 public:
  explicit LDAPHandler(LDAPConfig config);
  explicit LDAPHandler(LDAPConfig config, LDAPFunctions funcs);
  ~LDAPHandler() = default;

  // Non-copyable (holds config with credentials)
  LDAPHandler(LDAPHandler const&) = delete;
  LDAPHandler& operator=(LDAPHandler const&) = delete;

  /// Authenticate user against LDAP directory.
  /// Returns true on success, populates outRoles with mapped roles.
  /// Thread-safe: each call uses its own LDAP handle.
  bool authenticate(std::string const& username,
                    std::string const& password,
                    std::vector<std::string>& outRoles);

 private:
  /// Create a fresh LDAP* handle. Never stored as member.
  LDAP* createHandle();

  /// Destroy an LDAP* handle.
  void destroyHandle(LDAP* handle);

  /// Bind to LDAP as a specific user.
  bool bindUser(LDAP* handle, std::string const& dn,
                std::string const& password);

  /// Fetch roles for the authenticated user.
  std::vector<std::string> fetchRoles(LDAP* handle,
                                      std::string const& userDn);

  /// Fetch roles via the rolesAttribute method.
  std::vector<std::string> fetchRolesViaAttribute(LDAP* handle,
                                                  std::string const& userDn);

  /// Fetch roles via the rolesSearch method.
  std::vector<std::string> fetchRolesViaSearch(LDAP* handle,
                                               std::string const& userDn);

  LDAPConfig _config;
  LDAPFunctions _funcs;
};

}  // namespace arangodb

#endif  // ARANGODB_LDAP_HANDLER_H
