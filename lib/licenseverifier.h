// SPDX-License-Identifier: MIT
// High-level license verification: ties together .lic parsing (LicenseFile),
// signature math (LicenseCrypto), and a public key (caller-provided or the
// embedded application key).
//
// Typical use (against the embedded application public key):
//   LicensePayload payload;
//   if (LicenseVerifier::verifyContent(licText, &payload)) { /* valid */ }
//
// Test/alternative use (against a caller-supplied public key — decouples
// the test from whatever key happens to be embedded in licensepublickey.h):
//   if (LicenseVerifier::verifyContent(licText, pubkey, &payload)) { /* ... */ }

#pragma once

#include <QByteArray>
#include <QString>

struct LicensePayload;

namespace LicenseVerifier {

// Verify against the embedded application public key.
bool verifyContent(const QByteArray& licJsonText,
                   LicensePayload* payloadOut = nullptr);

// QString overload for the above.
bool verifyContent(const QString& licJsonText,
                   LicensePayload* payloadOut = nullptr);

// Verify against an arbitrary 32-byte public key. Useful for tests so they
// don't have to depend on whatever key is currently embedded in the app.
bool verifyContent(const QByteArray& licJsonText,
                   const QByteArray& publicKey,
                   LicensePayload* payloadOut = nullptr);

// QString overload for the above.
bool verifyContent(const QString& licJsonText,
                   const QByteArray& publicKey,
                   LicensePayload* payloadOut = nullptr);

} // namespace LicenseVerifier

