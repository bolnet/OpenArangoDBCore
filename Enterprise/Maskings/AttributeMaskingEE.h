#pragma once
#ifndef ARANGODB_ATTRIBUTE_MASKING_EE_H
#define ARANGODB_ATTRIBUTE_MASKING_EE_H

namespace arangodb::maskings {

/// Registers enterprise masking strategies:
///   - "xifyFront": replaces characters with 'x', preserving spaces/length
///   - "email": hashes email into AAAA.BBBB@CCCC.invalid format
///   - "creditCard": masks all but last 4 digits
///   - "phone": masks all but last 4 digits, preserving separators
///
/// Called from arangodump.cpp after InstallMaskings().
void InstallMaskingsEE();

}  // namespace arangodb::maskings

#endif  // ARANGODB_ATTRIBUTE_MASKING_EE_H
