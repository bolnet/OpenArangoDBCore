#include "Enterprise/Auth/LDAPHandler.h"

#include <cstring>
#include <string>
#include <vector>

namespace arangodb {

LDAPHandler::LDAPHandler(LDAPConfig config)
    : _config(std::move(config)), _funcs{} {
#ifdef ARANGODB_HAVE_LDAP
  // Production: wire up real libldap functions
  _funcs.initialize = [](LDAP** ldp, char const* uri) {
    return ldap_initialize(ldp, uri);
  };
  _funcs.simpleBind = [](LDAP* ld, char const* dn, char const* passwd) {
    return ldap_simple_bind_s(ld, dn, passwd);
  };
  _funcs.searchExtS = [](LDAP* ld, char const* base, int scope,
                          char const* filter, char** attrs, int attrsonly,
                          void* sctrls, void* cctrls, void* timeout,
                          int sizelimit, LDAPMessage** res) {
    return ldap_search_ext_s(ld, base, scope, filter, attrs, attrsonly,
                             (LDAPControl**)sctrls, (LDAPControl**)cctrls,
                             (struct timeval*)timeout, sizelimit, res);
  };
  _funcs.setOption = [](LDAP* ld, int option, void const* value) {
    return ldap_set_option(ld, option, value);
  };
  _funcs.startTlsS = [](LDAP* ld, void* sctrls, void* cctrls) {
    return ldap_start_tls_s(ld, (LDAPControl**)sctrls, (LDAPControl**)cctrls);
  };
  _funcs.unbindExt = [](LDAP* ld, void* sctrls, void* cctrls) {
    return ldap_unbind_ext(ld, (LDAPControl**)sctrls, (LDAPControl**)cctrls);
  };
  _funcs.getDn = [](LDAP* ld, LDAPMessage* entry) {
    return ldap_get_dn(ld, entry);
  };
  _funcs.memfree = [](void* p) { ldap_memfree(p); };
  _funcs.firstEntry = [](LDAP* ld, LDAPMessage* chain) {
    return ldap_first_entry(ld, chain);
  };
  _funcs.getValuesLen = [](LDAP* ld, LDAPMessage* entry, char const* attr) {
    return ldap_get_values_len(ld, entry, attr);
  };
  _funcs.valueFreeLen = [](BerValue** vals) { ldap_value_free_len(vals); };
  _funcs.msgfree = [](LDAPMessage* msg) { ldap_msgfree(msg); };
#endif
}

LDAPHandler::LDAPHandler(LDAPConfig config, LDAPFunctions funcs)
    : _config(std::move(config)), _funcs(std::move(funcs)) {}

// ---------------------------------------------------------------------------
// createHandle: Build URI, call ldap_initialize, configure TLS if needed.
// CRITICAL: Returns a fresh LDAP* -- never stored as member.
// ---------------------------------------------------------------------------
LDAP* LDAPHandler::createHandle() {
  // Build URI: ldaps:// for LDAPS mode, ldap:// otherwise
  std::string scheme = _config.useLDAPS ? "ldaps" : "ldap";
  std::string uri = scheme + "://" + _config.server + ":" +
                    std::to_string(_config.port);

  LDAP* handle = nullptr;
  int rc = _funcs.initialize(&handle, uri.c_str());
  if (rc != LDAP_SUCCESS || handle == nullptr) {
    return nullptr;
  }

  // If CA cert file is specified, set it before TLS negotiation
  if (!_config.tlsCACertFile.empty()) {
    _funcs.setOption(handle, LDAP_OPT_X_TLS_CACERTFILE,
                     _config.tlsCACertFile.c_str());
  }

  // StartTLS (on port 389, over ldap://)
  if (_config.useTLS && !_config.useLDAPS) {
    rc = _funcs.startTlsS(handle, nullptr, nullptr);
    if (rc != LDAP_SUCCESS) {
      destroyHandle(handle);
      return nullptr;
    }
  }

  return handle;
}

// ---------------------------------------------------------------------------
// destroyHandle: Unbind and free LDAP* handle.
// ---------------------------------------------------------------------------
void LDAPHandler::destroyHandle(LDAP* handle) {
  if (handle != nullptr) {
    _funcs.unbindExt(handle, nullptr, nullptr);
  }
}

// ---------------------------------------------------------------------------
// bindUser: Simple bind with DN and password.
// ---------------------------------------------------------------------------
bool LDAPHandler::bindUser(LDAP* handle, std::string const& dn,
                           std::string const& password) {
  if (handle == nullptr) {
    return false;
  }
  int rc = _funcs.simpleBind(handle, dn.c_str(), password.c_str());
  return rc == LDAP_SUCCESS;
}

// ---------------------------------------------------------------------------
// authenticate: Main entry point. Per-request handle pattern.
// ---------------------------------------------------------------------------
bool LDAPHandler::authenticate(std::string const& username,
                                std::string const& password,
                                std::vector<std::string>& outRoles) {
  outRoles.clear();

  if (_config.mode == LDAPAuthMode::Simple) {
    // Simple mode: construct DN = prefix + username + suffix
    std::string dn = _config.prefix + username + _config.suffix;

    LDAP* handle = createHandle();
    if (handle == nullptr) {
      return false;
    }

    bool bound = bindUser(handle, dn, password);
    if (!bound) {
      destroyHandle(handle);
      return false;
    }

    // Fetch roles using the same handle
    outRoles = fetchRoles(handle, dn);
    destroyHandle(handle);
    return true;

  } else {
    // Search mode: admin bind, search for user DN, then rebind as user

    // Step 1: Create handle and bind as admin
    LDAP* adminHandle = createHandle();
    if (adminHandle == nullptr) {
      return false;
    }

    if (!bindUser(adminHandle, _config.binddn, _config.bindpasswd)) {
      destroyHandle(adminHandle);
      return false;
    }

    // Step 2: Search for the user DN
    std::string filter = "(uid=" + username + ")";
    LDAPMessage* searchResult = nullptr;
    int rc = _funcs.searchExtS(adminHandle, _config.basedn.c_str(),
                                LDAP_SCOPE_SUBTREE, filter.c_str(),
                                nullptr, 0, nullptr, nullptr, nullptr, 0,
                                &searchResult);

    std::string userDn;
    if (rc == LDAP_SUCCESS && searchResult != nullptr) {
      LDAPMessage* entry = _funcs.firstEntry(adminHandle, searchResult);
      if (entry != nullptr) {
        char* dn = _funcs.getDn(adminHandle, entry);
        if (dn != nullptr) {
          userDn = dn;
          _funcs.memfree(dn);
        }
      }
      _funcs.msgfree(searchResult);
    }

    destroyHandle(adminHandle);

    if (userDn.empty()) {
      return false;
    }

    // Step 3: Create new handle and bind as the found user
    LDAP* userHandle = createHandle();
    if (userHandle == nullptr) {
      return false;
    }

    if (!bindUser(userHandle, userDn, password)) {
      destroyHandle(userHandle);
      return false;
    }

    // Step 4: Fetch roles
    outRoles = fetchRoles(userHandle, userDn);
    destroyHandle(userHandle);
    return true;
  }
}

// ---------------------------------------------------------------------------
// fetchRoles: Dispatch to attribute or search method.
// ---------------------------------------------------------------------------
std::vector<std::string> LDAPHandler::fetchRoles(LDAP* handle,
                                                  std::string const& userDn) {
  if (!_config.rolesAttribute.empty()) {
    return fetchRolesViaAttribute(handle, userDn);
  }
  if (!_config.rolesSearch.empty()) {
    return fetchRolesViaSearch(handle, userDn);
  }
  return {};
}

// ---------------------------------------------------------------------------
// fetchRolesViaAttribute: Read roles from user's LDAP attribute.
// ---------------------------------------------------------------------------
std::vector<std::string> LDAPHandler::fetchRolesViaAttribute(
    LDAP* handle, std::string const& userDn) {
  std::vector<std::string> roles;

  // Search for the user entry to read the roles attribute
  std::string filter = "(objectClass=*)";
  LDAPMessage* result = nullptr;
  int rc = _funcs.searchExtS(handle, userDn.c_str(), LDAP_SCOPE_SUBTREE,
                              filter.c_str(), nullptr, 0, nullptr, nullptr,
                              nullptr, 0, &result);

  if (rc != LDAP_SUCCESS || result == nullptr) {
    return roles;
  }

  LDAPMessage* entry = _funcs.firstEntry(handle, result);
  if (entry != nullptr) {
    BerValue** vals = _funcs.getValuesLen(handle, entry,
                                          _config.rolesAttribute.c_str());
    if (vals != nullptr) {
      for (int i = 0; vals[i] != nullptr; ++i) {
        if (vals[i]->bv_val != nullptr && vals[i]->bv_len > 0) {
          roles.emplace_back(vals[i]->bv_val,
                             static_cast<size_t>(vals[i]->bv_len));
        }
      }
      _funcs.valueFreeLen(vals);
    }
  }

  _funcs.msgfree(result);
  return roles;
}

// ---------------------------------------------------------------------------
// fetchRolesViaSearch: Search for groups where user is a member.
// ---------------------------------------------------------------------------
std::vector<std::string> LDAPHandler::fetchRolesViaSearch(
    LDAP* handle, std::string const& userDn) {
  std::vector<std::string> roles;

  // Replace {userDn} placeholder in the search filter
  std::string filter = _config.rolesSearch;
  std::string placeholder = "{userDn}";
  auto pos = filter.find(placeholder);
  if (pos != std::string::npos) {
    filter.replace(pos, placeholder.size(), userDn);
  }

  LDAPMessage* result = nullptr;
  int rc = _funcs.searchExtS(handle, _config.basedn.c_str(),
                              LDAP_SCOPE_SUBTREE, filter.c_str(),
                              nullptr, 0, nullptr, nullptr, nullptr, 0,
                              &result);

  if (rc == LDAP_SUCCESS && result != nullptr) {
    // Each result entry is a group; use its DN as the role name
    LDAPMessage* entry = _funcs.firstEntry(handle, result);
    if (entry != nullptr) {
      char* dn = _funcs.getDn(handle, entry);
      if (dn != nullptr) {
        roles.emplace_back(dn);
        _funcs.memfree(dn);
      }
    }
    _funcs.msgfree(result);
  }

  return roles;
}

}  // namespace arangodb
