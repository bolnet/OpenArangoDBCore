#include "Enterprise/Auth/LDAPHandler.h"

namespace arangodb {

LDAPHandler::LDAPHandler(LDAPConfig config)
    : _config(std::move(config)), _funcs{} {}

LDAPHandler::LDAPHandler(LDAPConfig config, LDAPFunctions funcs)
    : _config(std::move(config)), _funcs(std::move(funcs)) {}

bool LDAPHandler::authenticate(std::string const& /*username*/,
                                std::string const& /*password*/,
                                std::vector<std::string>& /*outRoles*/) {
  // TODO: implement
  return false;
}

LDAP* LDAPHandler::createHandle() {
  // TODO: implement
  return nullptr;
}

void LDAPHandler::destroyHandle(LDAP* /*handle*/) {
  // TODO: implement
}

bool LDAPHandler::bindUser(LDAP* /*handle*/, std::string const& /*dn*/,
                           std::string const& /*password*/) {
  // TODO: implement
  return false;
}

std::vector<std::string> LDAPHandler::fetchRoles(
    LDAP* /*handle*/, std::string const& /*userDn*/) {
  // TODO: implement
  return {};
}

std::vector<std::string> LDAPHandler::fetchRolesViaAttribute(
    LDAP* /*handle*/, std::string const& /*userDn*/) {
  // TODO: implement
  return {};
}

std::vector<std::string> LDAPHandler::fetchRolesViaSearch(
    LDAP* /*handle*/, std::string const& /*userDn*/) {
  // TODO: implement
  return {};
}

}  // namespace arangodb
