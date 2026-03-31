#pragma once
#ifndef ARANGODB_LDAP_CONFIG_H
#define ARANGODB_LDAP_CONFIG_H

#include <string>

namespace arangodb {

enum class LDAPAuthMode { Simple, Search };

struct LDAPConfig {
  std::string server;            // --ldap.server
  int port = 389;                // --ldap.port
  std::string basedn;            // --ldap.basedn
  std::string binddn;            // --ldap.binddn (admin bind for search mode)
  std::string bindpasswd;        // --ldap.bindpasswd
  std::string prefix;            // --ldap.prefix (simple mode DN prefix)
  std::string suffix;            // --ldap.suffix (simple mode DN suffix)
  bool useTLS = false;           // --ldap.tls (StartTLS on port 389)
  bool useLDAPS = false;         // ldaps:// scheme on port 636
  std::string tlsCACertFile;     // --ldap.tls-cacert-file
  std::string rolesAttribute;    // --ldap.roles-attribute-name
  std::string rolesSearch;       // --ldap.roles-search filter
  LDAPAuthMode mode = LDAPAuthMode::Simple;

  /// Validate that required fields are populated.
  /// Returns empty string on success, error description on failure.
  std::string validate() const;
};

}  // namespace arangodb

#endif  // ARANGODB_LDAP_CONFIG_H
