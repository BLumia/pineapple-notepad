#include "licensefile.h"

#include "stringhelpers.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

using namespace Qt::StringLiterals;

namespace LicenseFile {

QByteArray canonicalBytes(const LicensePayload& p) {
    QString s;
    s.reserve(128);
    s += u"{\"email\":\""_s       + Helpers::jsonEscape(p.email)    + u"\""_s;
    s += u",\"issued_at\":\""_s   + Helpers::jsonEscape(p.issuedAt) + u"\""_s;
    s += u",\"licensee\":\""_s    + Helpers::jsonEscape(p.licensee) + u"\""_s;
    s += u",\"tags\":["_s;
    for (int i = 0; i < p.tags.size(); ++i) {
        if (i > 0) s += u","_s;
        s += u"\""_s + Helpers::jsonEscape(p.tags[i]) + u"\""_s;
    }
    s += u"]}"_s;
    return s.toUtf8();
}

bool save(const QString& path,
          const LicensePayload& payload,
          const QByteArray& signature) {
    QJsonObject payloadObj;
    payloadObj.insert(u"email"_s,     payload.email);
    payloadObj.insert(u"issued_at"_s, payload.issuedAt);
    payloadObj.insert(u"licensee"_s,  payload.licensee);
    QJsonArray tagsArray;
    for (const QString& tag : payload.tags) {
        tagsArray.append(tag);
    }
    payloadObj.insert(u"tags"_s, tagsArray);

    QJsonObject root;
    root.insert(u"payload"_s, payloadObj);
    root.insert(u"sig"_s, Helpers::toBase64(signature));

    QJsonDocument doc(root);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    f.write(doc.toJson());
    f.close();
    return f.error() == QFile::NoError;
}

bool load(const QString& path,
          LicensePayload* payloadOut,
          QByteArray* signatureOut) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray raw = f.readAll();
    f.close();

    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;

    QJsonObject root = doc.object();
    QJsonObject payloadObj = root.value(u"payload"_s).toObject();
    if (payloadObj.isEmpty()) return false;

    LicensePayload p;
    p.email     = payloadObj.value(u"email"_s).toString();
    p.issuedAt  = payloadObj.value(u"issued_at"_s).toString();
    p.licensee  = payloadObj.value(u"licensee"_s).toString();
    const QJsonArray tagsArr = payloadObj.value(u"tags"_s).toArray();
    for (const QJsonValue& v : tagsArr) {
        p.tags.append(v.toString());
    }

    const QString sigB64 = root.value(u"sig"_s).toString();
    const QByteArray sig = Helpers::fromBase64(sigB64);
    if (sig.size() != 64) return false;

    *payloadOut = p;
    *signatureOut = sig;
    return true;
}

} // namespace LicenseFile
