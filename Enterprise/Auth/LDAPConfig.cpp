#include "Enterprise/Auth/LDAPConfig.h"

namespace arangodb {

std::string LDAPConfig::validate() const {
  if (server.empty()) {
    return "LDAP server address must not be empty";
  }
  if (port < 1 || port > 65535) {
    return "LDAP port must be between 1 and 65535";
  }
  if (mode == LDAPAuthMode::Search) {
    if (binddn.empty()) {
      return "binddn is required for search authentication mode";
    }
    if (basedn.empty()) {
      return "basedn is required for search authentication mode";
    }
  }
  if (useTLS && useLDAPS) {
    return "Cannot enable both StartTLS and LDAPS simultaneously";
  }
  return "";
}

}  // namespace arangodb
