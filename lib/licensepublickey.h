// SPDX-License-Identifier: MIT
// Embedded Ed25519 (SHA-512, RFC 8032) public key used to verify imported
// .lic files.
//
// The matching private key (32-byte seed) is held offline and is NOT part of
// the application. Only the public key is embedded here.
//
// TEST KEYPAIR — will be replaced with a production keypair before release.
//   seed    (32B): offline/test_seed.hex        (private, do not ship)
//   pubkey  (32B): offline/test_pubkey.hex       (public, embedded below)

#pragma once

#include <QByteArray>
#include <QString>

namespace LicensePublicKey {

// 32-byte Ed25519 public key, hex-encoded (64 lowercase chars).
// Keep in sync with offline/test_pubkey.hex.
inline constexpr const char* kHex =
    "d431249efda61767eae1f4d732b3adf41adb2784773687033115cedfce4bf65f";

// The 32 raw bytes, parsed at call time.
inline QByteArray bytes()
{
    return QByteArray::fromHex(QByteArray::fromRawData(kHex, 64));
}

} // namespace LicensePublicKey
