// SPDX-License-Identifier: MIT
// License file (.lic) read/write + canonical serialization.
//
// On-disk format (indented JSON for human readability):
//
//   {
//     "payload": {
//       "email":     "user@example.com",
//       "issued_at": "2025-07-17T20:00:00Z",
//       "licensee":  "sample user",
//       "tags":      ["tag1", "tag2"]
//     },
//     "sig": "<base64 EdDSA signature>"
//   }
//
// Canonical form (what actually gets signed — sorted keys, compact):
//
//   {"email":"...","issued_at":"...","licensee":"...","tags":["..."]}

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

struct LicensePayload {
    QString licensee;
    QString email;
    QString issuedAt;   // ISO 8601 UTC, e.g. "2025-07-17T20:00:00Z"
    QStringList tags;
};

namespace LicenseFile {

// Build the canonical JSON byte sequence (UTF-8) that gets signed.
// Keys are emitted in alphabetical order, matching Python's
// `json.dumps(payload, sort_keys=True, separators=(",", ":"))`.
QByteArray canonicalBytes(const LicensePayload& payload);

// Write a deployable .lic file to disk. Returns true on success.
// The signature must be 64 raw bytes; it will be base64-encoded in the file.
bool save(const QString& path,
          const LicensePayload& payload,
          const QByteArray& signature);

// Parse a .lic file. On success, fills *payloadOut and *signatureOut
// (signature returned as raw 64 bytes) and returns true.
bool load(const QString& path,
          LicensePayload* payloadOut,
          QByteArray* signatureOut);

} // namespace LicenseFile
