// SPDX-License-Identifier: MIT

#include "stringhelpers.h"

#include <QCoreApplication>
#include <QDir>
#include <QRegularExpression>

using namespace Qt::StringLiterals;

namespace Helpers {

QString toHex(const QByteArray& bytes) {
    return QString::fromLatin1(bytes.toHex());
}

QByteArray fromHex(const QString& hex) {
    QString cleaned = hex;
    cleaned.remove(QRegularExpression(u"[\\s]"_s));   // strip whitespace
    if (cleaned.startsWith(u"0x"_s, Qt::CaseInsensitive)) {
        cleaned = cleaned.mid(2);
    }
    if (cleaned.size() % 2 != 0) return {};
    return QByteArray::fromHex(cleaned.toLatin1());
}

bool isValidSeedHex(const QString& s) {
    QString cleaned = s;
    cleaned.remove(QRegularExpression(u"[\\s]"_s));
    if (cleaned.startsWith(u"0x"_s, Qt::CaseInsensitive)) {
        cleaned = cleaned.mid(2);
    }
    QRegularExpression re(u"^[0-9a-fA-F]{64}$"_s);
    return re.match(cleaned).hasMatch();
}

QString toBase64(const QByteArray& bytes) {
    return QString::fromLatin1(bytes.toBase64());
}

QByteArray fromBase64(const QString& b64) {
    return QByteArray::fromBase64(b64.toLatin1());
}

QString jsonEscape(const QString& s) {
    QString out;
    out.reserve(s.size() + 8);
    for (QChar ch : s) {
        ushort u = ch.unicode();
        switch (u) {
            case u'"':  out += u"\\\""_s; break;
            case u'\\': out += u"\\\\"_s;  break;
            case u'\b': out += u"\\b"_s;   break;
            case u'\f': out += u"\\f"_s;   break;
            case u'\n': out += u"\\n"_s;   break;
            case u'\r': out += u"\\r"_s;   break;
            case u'\t': out += u"\\t"_s;   break;
            default:
                if (u < 0x20 || u >= 0x7f) {
                    out += u"\\u"_s + QString(u"%1"_s).arg(u, 4, 16, QChar(u'0'));
                } else {
                    out += ch;
                }
        }
    }
    return out;
}

QString sanitizeFilename(const QString& s) {
    QString trimmed = s.trimmed();
    if (trimmed.isEmpty()) return u"license"_s;
    QString out;
    out.reserve(trimmed.size());
    for (QChar ch : trimmed) {
        ushort u = ch.unicode();
        bool allowed =
            (u >= u'a' && u <= u'z') ||
            (u >= u'A' && u <= u'Z') ||
            (u >= u'0' && u <= u'9') ||
            u == u'.' || u == u'@' || u == u'_' || u == u'+';
        out += allowed ? ch : u'_';
    }
    if (out.isEmpty()) return u"license"_s;
    return out;
}

QString exeDir() {
    return QCoreApplication::applicationDirPath();
}

} // namespace Helpers
