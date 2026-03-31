#include "AuditFeature.h"

#include <type_traits>

static_assert(!std::is_abstract_v<arangodb::AuditFeature>,
              "AuditFeature must not be abstract");

namespace arangodb {
// All lifecycle method bodies are defined inline in AuditFeature.h.
}  // namespace arangodb
