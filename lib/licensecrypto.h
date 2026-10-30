// SPDX-License-Identifier: MIT
// RAII-free thin C++ wrapper around monocypher's standard Ed25519 (SHA-512)
// API.  Standard Ed25519 (RFC 8032) is interoperable with Go's crypto/ed25519,
// PyNaCl, JS @noble/ed25519, WebCrypto, etc.

#pragma once

#include <QByteArray>

namespace LicenseCrypto {

constexpr int kSeedSize = 32;        // 32-byte random seed (the persistent private key)
constexpr int kPublicKeySize = 32;   // public key derived from the seed
constexpr int kSecretKeySize = 64;   // expanded secret key = seed || pubkey
constexpr int kSignatureSize = 64;   // Ed25519 signature is always 64 bytes

// Generate 32 cryptographically secure random bytes using the OS CSPRNG
// (QRandomGenerator::system() uses BCryptGenRandom on Windows,
//  getrandom/urandom on Linux).
QByteArray generateRandomSeed();

// Derive the 32-byte public key from a 32-byte seed.
// Returns an empty QByteArray if the input is not exactly 32 bytes.
QByteArray derivePublicKey(const QByteArray& seed);

// Derive the 64-byte expanded secret key (seed || public_key) from a 32-byte seed.
// Returns an empty QByteArray on bad input.
QByteArray deriveSecretKey(const QByteArray& seed);

// Sign a message with the 64-byte secret key. Returns the 64-byte signature,
// or an empty QByteArray on bad input.
QByteArray sign(const QByteArray& secretKey, const QByteArray& message);

// Verify a signature against a 32-byte public key.
// Returns true iff the signature is valid (crypto_ed25519_check returns 0).
bool verify(const QByteArray& signature,
            const QByteArray& publicKey,
            const QByteArray& message);

} // namespace LicenseCrypto
