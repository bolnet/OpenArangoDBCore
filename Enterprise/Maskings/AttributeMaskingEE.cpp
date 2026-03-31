#include "AttributeMaskingEE.h"
#include "AttributeMasking.h"

#include <memory>

namespace arangodb::maskings {

void InstallMaskingsEE() {
  AttributeMasking::installMasking("xifyFront", []() {
    return std::make_unique<XifyFrontMask>();
  });
  AttributeMasking::installMasking("email", []() {
    return std::make_unique<EmailMask>();
  });
  AttributeMasking::installMasking("creditCard", []() {
    return std::make_unique<CreditCardMask>();
  });
  AttributeMasking::installMasking("phone", []() {
    return std::make_unique<PhoneMask>();
  });
}

}  // namespace arangodb::maskings
