// SPDX-License-Identifier: MIT
// Small string/encoding helpers used by the license code.

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace Helpers {

// ---------- hex ----------
// Lowercase hex string, no separators.
QString toHex(const QByteArray& bytes);

// Parse hex string into bytes. Strips whitespace and an optional "0x" prefix.
// Returns empty QByteArray if the cleaned input is not an even number of hex chars.
QByteArray fromHex(const QString& hex);

// True iff the cleaned input is exactly 64 hex characters (32 bytes).
bool isValidSeedHex(const QString& s);


// ---------- base64 ----------
QString toBase64(const QByteArray& bytes);
QByteArray fromBase64(const QString& b64);


// ---------- JSON ----------
// JSON string escape matching Python's json.dumps default (ensure_ascii=True):
// - escapes ", \, control chars
// - emits \uXXXX for any non-ASCII UTF-16 code unit (matches Python's
//   surrogate-pair output for code points outside the BMP).
QString jsonEscape(const QString& s);


// ---------- filesystem ----------
// Sanitize an email-like string into a safe filename component (no path
// separators, no Windows-forbidden chars). Allowed set: [A-Za-z0-9.@_+-].
// Everything else becomes a single '_'.
QString sanitizeFilename(const QString& s);


// ---------- file paths ----------
// The directory containing the running executable. All generated files
// (private_key.hex, public_key.hex, *.lic) live here.
QString exeDir();

} // namespace Helpers
