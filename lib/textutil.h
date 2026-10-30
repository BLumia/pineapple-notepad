// SPDX-License-Identifier: MIT
// UTF-8 decoding helper that keeps UTF-16 (QString) positions mapped back to
// byte offsets, so ranges computed on the decoded text can be translated into
// Scintilla document byte positions. Shared by the URL scanner
// (SciEdit) and the spell check controller.

#pragma once

#include <QByteArray>
#include <QChar>
#include <QVector>

struct Utf8DecodedText {
    // UTF-16 code units of the decoded text.
    QVector<QChar> chars;
    // bytePositions[i] is the byte offset of chars[i]; the final entry is the
    // total byte count, so bytePositions.size() == chars.size() + 1.
    QVector<int> bytePositions;
};

inline Utf8DecodedText decodeUtf8WithBytePositions(const QByteArray &bytes)
{
    Utf8DecodedText result;
    result.chars.reserve(bytes.size());
    result.bytePositions.reserve(bytes.size() + 1);

    int i = 0;
    while (i < bytes.size()) {
        const int startByte = i;
        const unsigned char c0 = static_cast<unsigned char>(bytes.at(i));
        uint32_t codepoint = 0;
        int length = 1;

        if (c0 < 0x80) {
            codepoint = c0;
            length = 1;
        } else if ((c0 & 0xE0) == 0xC0 && i + 1 < bytes.size()) {
            const unsigned char c1 = static_cast<unsigned char>(bytes.at(i + 1));
            if ((c1 & 0xC0) == 0x80) {
                codepoint = ((c0 & 0x1F) << 6) | (c1 & 0x3F);
                length = 2;
                if (codepoint < 0x80) {
                    codepoint = 0xFFFD;
                    length = 1;
                }
            } else {
                codepoint = 0xFFFD;
                length = 1;
            }
        } else if ((c0 & 0xF0) == 0xE0 && i + 2 < bytes.size()) {
            const unsigned char c1 = static_cast<unsigned char>(bytes.at(i + 1));
            const unsigned char c2 = static_cast<unsigned char>(bytes.at(i + 2));
            if (((c1 & 0xC0) == 0x80) && ((c2 & 0xC0) == 0x80)) {
                codepoint = ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
                length = 3;
                if (codepoint < 0x800) {
                    codepoint = 0xFFFD;
                    length = 1;
                }
            } else {
                codepoint = 0xFFFD;
                length = 1;
            }
        } else if ((c0 & 0xF8) == 0xF0 && i + 3 < bytes.size()) {
            const unsigned char c1 = static_cast<unsigned char>(bytes.at(i + 1));
            const unsigned char c2 = static_cast<unsigned char>(bytes.at(i + 2));
            const unsigned char c3 = static_cast<unsigned char>(bytes.at(i + 3));
            if (((c1 & 0xC0) == 0x80) && ((c2 & 0xC0) == 0x80) && ((c3 & 0xC0) == 0x80)) {
                codepoint = ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                length = 4;
                if (codepoint < 0x10000 || codepoint > 0x10FFFF) {
                    codepoint = 0xFFFD;
                    length = 1;
                }
            } else {
                codepoint = 0xFFFD;
                length = 1;
            }
        } else {
            codepoint = 0xFFFD;
            length = 1;
        }

        if (codepoint <= 0xFFFF) {
            result.chars.append(QChar(static_cast<ushort>(codepoint)));
            result.bytePositions.append(startByte);
        } else {
            const uint32_t adjusted = codepoint - 0x10000;
            const ushort high = static_cast<ushort>(0xD800 + (adjusted >> 10));
            const ushort low = static_cast<ushort>(0xDC00 + (adjusted & 0x3FF));
            result.chars.append(QChar(high));
            result.bytePositions.append(startByte);
            result.chars.append(QChar(low));
            result.bytePositions.append(startByte);
        }

        i += length;
    }

    result.bytePositions.append(bytes.size());
    return result;
}
