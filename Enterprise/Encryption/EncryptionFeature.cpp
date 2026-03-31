#include "EncryptionFeature.h"

#include <type_traits>

static_assert(!std::is_abstract_v<arangodb::EncryptionFeature>,
              "EncryptionFeature must not be abstract");

namespace arangodb {
// All lifecycle method bodies are defined inline in EncryptionFeature.h.
}  // namespace arangodb
