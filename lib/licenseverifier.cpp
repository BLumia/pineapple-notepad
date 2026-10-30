#include "licenseverifier.h"

#include "licensecrypto.h"
#include "licensefile.h"
#include "licensepublickey.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace LicenseVerifier {

bool verifyContent(const QByteArray& licJsonText,
                   const QByteArray& publicKey,
                   LicensePayload* payloadOut)
{
    // Parse the JSON wrapper to extract payload + signature without touching
    // the filesystem. We don't use LicenseFile::load() here because that
    // requires a path; this works on in-memory text (e.g. from kcfg).
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(licJsonText, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    const QJsonObject root = doc.object();
    const QJsonObject payloadObj = root.value(u"payload"_s).toObject();
    if (payloadObj.isEmpty())
        return false;

    LicensePayload p;
    p.email     = payloadObj.value(u"email"_s).toString();
    p.issuedAt  = payloadObj.value(u"issued_at"_s).toString();
    p.licensee  = payloadObj.value(u"licensee"_s).toString();
    const QJsonArray tagsArr = payloadObj.value(u"tags"_s).toArray();
    for (const QJsonValue& v : tagsArr)
        p.tags.append(v.toString());

    const QString sigB64 = root.value(u"sig"_s).toString();
    const QByteArray sig = QByteArray::fromBase64(sigB64.toLatin1());
    if (sig.size() != LicenseCrypto::kSignatureSize)
        return false;

    const QByteArray canonical = LicenseFile::canonicalBytes(p);
    const bool ok = LicenseCrypto::verify(sig, publicKey, canonical);
    if (!ok)
        return false;

    if (payloadOut)
        *payloadOut = p;
    return true;
}

bool verifyContent(const QByteArray& licJsonText, LicensePayload* payloadOut)
{
    return verifyContent(licJsonText, LicensePublicKey::bytes(), payloadOut);
}

bool verifyContent(const QString& licJsonText, LicensePayload* payloadOut)
{
    return verifyContent(licJsonText.toUtf8(), payloadOut);
}

bool verifyContent(const QString& licJsonText,
                   const QByteArray& publicKey,
                   LicensePayload* payloadOut)
{
    return verifyContent(licJsonText.toUtf8(), publicKey, payloadOut);
}

} // namespace LicenseVerifier
