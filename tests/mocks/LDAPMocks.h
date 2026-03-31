#pragma once
#ifndef ARANGODB_LDAP_MOCKS_H
#define ARANGODB_LDAP_MOCKS_H

/// Mock libldap C functions for testing without a real LDAP server.
///
/// Strategy: Function pointer indirection. The LDAPHandler implementation
/// calls through function pointers (ldap_*_fn) which default to real libldap
/// in production but are replaced by mocks in tests.
///
/// This header also provides the mock LDAP* type definition for platforms
/// where libldap is not installed (ARANGODB_HAVE_LDAP == 0).

#include <atomic>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// Mock LDAP types (replace <ldap.h> types in test builds)
// ---------------------------------------------------------------------------

/// Opaque LDAP handle (mock)
struct MockLDAPHandle {
  int id;
  bool bound = false;
  std::string boundDn;
};

/// Mock LDAP message entry
struct MockLDAPMessage {
  std::string dn;
  std::unordered_map<std::string, std::vector<std::string>> attributes;
};

/// Mock berval structure
struct MockBerval {
  char* bv_val;
  int bv_len;
};

// ---------------------------------------------------------------------------
// LDAP constants used by LDAPHandler
// ---------------------------------------------------------------------------
#ifndef LDAP_SUCCESS
#define LDAP_SUCCESS             0
#define LDAP_INVALID_CREDENTIALS 49
#define LDAP_NO_SUCH_OBJECT      32
#define LDAP_SCOPE_SUBTREE       0x0002
#define LDAP_OPT_X_TLS           0x6000
#define LDAP_OPT_X_TLS_CACERTFILE 0x6002
#define LDAP_OPT_ON              ((void*)1)
#endif

// Use MockLDAPHandle* as the LDAP* type
using LDAP = MockLDAPHandle;
using LDAPMessage = MockLDAPMessage;
using BerValue = MockBerval;

// ---------------------------------------------------------------------------
// Mock state (global, reset between tests)
// ---------------------------------------------------------------------------
namespace ldap_mock {

struct MockState {
  // Track calls
  std::vector<std::string> callLog;
  std::mutex mutex;

  // Configurable behavior
  int initializeResult = LDAP_SUCCESS;
  int bindResult = LDAP_SUCCESS;
  int searchResult = LDAP_SUCCESS;
  int startTlsResult = LDAP_SUCCESS;

  // Counter for unique handle IDs
  std::atomic<int> handleCounter{0};

  // Bind credentials that are considered valid
  std::unordered_map<std::string, std::string> validCredentials;

  // Search results to return
  std::vector<MockLDAPMessage> searchResults;

  // Track created/destroyed handles for leak detection
  std::atomic<int> activeHandles{0};

  void reset() {
    std::lock_guard<std::mutex> lock(mutex);
    callLog.clear();
    initializeResult = LDAP_SUCCESS;
    bindResult = LDAP_SUCCESS;
    searchResult = LDAP_SUCCESS;
    startTlsResult = LDAP_SUCCESS;
    handleCounter = 0;
    validCredentials.clear();
    searchResults.clear();
    activeHandles = 0;
  }

  void logCall(std::string const& call) {
    std::lock_guard<std::mutex> lock(mutex);
    callLog.push_back(call);
  }

  std::vector<std::string> getCalls() {
    std::lock_guard<std::mutex> lock(mutex);
    return callLog;
  }
};

inline MockState& state() {
  static MockState s;
  return s;
}

}  // namespace ldap_mock

// ---------------------------------------------------------------------------
// Mock libldap function implementations
// ---------------------------------------------------------------------------

inline int mock_ldap_initialize(LDAP** ldp, char const* uri) {
  ldap_mock::state().logCall(std::string("ldap_initialize:") + (uri ? uri : "null"));
  if (ldap_mock::state().initializeResult != LDAP_SUCCESS) {
    *ldp = nullptr;
    return ldap_mock::state().initializeResult;
  }
  auto* handle = new MockLDAPHandle();
  handle->id = ldap_mock::state().handleCounter.fetch_add(1);
  handle->bound = false;
  *ldp = handle;
  ldap_mock::state().activeHandles.fetch_add(1);
  return LDAP_SUCCESS;
}

inline int mock_ldap_simple_bind_s(LDAP* ld, char const* dn, char const* passwd) {
  ldap_mock::state().logCall(std::string("ldap_simple_bind_s:") +
                             (dn ? dn : "null") + ":" +
                             (passwd ? "***" : "null"));
  if (ldap_mock::state().bindResult != LDAP_SUCCESS) {
    return ldap_mock::state().bindResult;
  }
  // Check credentials if configured
  auto& creds = ldap_mock::state().validCredentials;
  std::string dnStr = dn ? dn : "";
  std::string passStr = passwd ? passwd : "";
  if (!creds.empty()) {
    auto it = creds.find(dnStr);
    if (it == creds.end() || it->second != passStr) {
      return LDAP_INVALID_CREDENTIALS;
    }
  }
  ld->bound = true;
  ld->boundDn = dnStr;
  return LDAP_SUCCESS;
}

inline int mock_ldap_search_ext_s(LDAP* ld, char const* base, int scope,
                                  char const* filter, char** attrs,
                                  int attrsonly, void* /*serverctrls*/,
                                  void* /*clientctrls*/, void* /*timeout*/,
                                  int /*sizelimit*/, LDAPMessage** res) {
  ldap_mock::state().logCall(std::string("ldap_search_ext_s:") +
                             (base ? base : "null") + ":" +
                             (filter ? filter : "null"));
  if (ldap_mock::state().searchResult != LDAP_SUCCESS) {
    *res = nullptr;
    return ldap_mock::state().searchResult;
  }
  // Return first search result if available
  if (!ldap_mock::state().searchResults.empty()) {
    *res = new MockLDAPMessage(ldap_mock::state().searchResults[0]);
  } else {
    *res = nullptr;
    return LDAP_NO_SUCH_OBJECT;
  }
  return LDAP_SUCCESS;
}

inline int mock_ldap_set_option(LDAP* /*ld*/, int option, void const* /*value*/) {
  ldap_mock::state().logCall("ldap_set_option:" + std::to_string(option));
  return LDAP_SUCCESS;
}

inline int mock_ldap_start_tls_s(LDAP* /*ld*/, void* /*serverctrls*/,
                                 void* /*clientctrls*/) {
  ldap_mock::state().logCall("ldap_start_tls_s");
  return ldap_mock::state().startTlsResult;
}

inline int mock_ldap_unbind_ext(LDAP* ld, void* /*sctrls*/, void* /*cctrls*/) {
  ldap_mock::state().logCall("ldap_unbind_ext:" + std::to_string(ld->id));
  ldap_mock::state().activeHandles.fetch_sub(1);
  delete ld;
  return LDAP_SUCCESS;
}

inline char* mock_ldap_get_dn(LDAP* /*ld*/, LDAPMessage* entry) {
  if (!entry) return nullptr;
  // Return a copy that must be freed with ldap_memfree
  char* dn = new char[entry->dn.size() + 1];
  std::strcpy(dn, entry->dn.c_str());
  return dn;
}

inline void mock_ldap_memfree(void* p) {
  delete[] static_cast<char*>(p);
}

inline LDAPMessage* mock_ldap_first_entry(LDAP* /*ld*/, LDAPMessage* chain) {
  return chain;  // In mock, message IS the first entry
}

/// Mock ldap_get_values_len - returns attribute values as BerValue array
/// Caller must free with mock_ldap_value_free_len
inline BerValue** mock_ldap_get_values_len(LDAP* /*ld*/, LDAPMessage* entry,
                                           char const* attr) {
  if (!entry || !attr) return nullptr;
  std::string attrStr(attr);
  auto it = entry->attributes.find(attrStr);
  if (it == entry->attributes.end() || it->second.empty()) return nullptr;

  auto const& vals = it->second;
  // Allocate array of BerValue* + null terminator
  auto** result = new BerValue*[vals.size() + 1];
  for (size_t i = 0; i < vals.size(); ++i) {
    result[i] = new BerValue();
    result[i]->bv_val = new char[vals[i].size() + 1];
    std::strcpy(result[i]->bv_val, vals[i].c_str());
    result[i]->bv_len = static_cast<int>(vals[i].size());
  }
  result[vals.size()] = nullptr;
  return result;
}

inline void mock_ldap_value_free_len(BerValue** vals) {
  if (!vals) return;
  for (int i = 0; vals[i] != nullptr; ++i) {
    delete[] vals[i]->bv_val;
    delete vals[i];
  }
  delete[] vals;
}

inline void mock_ldap_msgfree(LDAPMessage* msg) {
  delete msg;
}

#endif  // ARANGODB_LDAP_MOCKS_H
