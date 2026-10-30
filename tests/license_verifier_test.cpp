/*
 * Tests for LicenseVerifier::verifyContent().
 *
 * Generates its own EdDSA keypair at runtime (via LicenseCrypto) so the test
 * suite is decoupled from whatever public key happens to be embedded in
 * lib/licensepublickey.h. This means swapping the production keypair doesn't
 * break the test, and tampering can be checked against a known-good baseline.
 */
#include "licensecrypto.h"
#include "licensefile.h"
#include "licenseverifier.h"

#include <QtTest/QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::StringLiterals;

class LicenseVerifierTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void validLicense_verifies();
    void validLicense_fillsPayload();
    void tamperedLicensee_rejected();
    void tamperedSignature_rejected();
    void emptyContent_rejected();
    void invalidJson_rejected();
    void missingPayload_rejected();

private:
    // Build a .lic JSON document signed with our test key.
    QByteArray buildLic(const LicensePayload& p) const;

    // Runtime-generated keypair for this test run.
    QByteArray m_publicKey;
    QByteArray m_secretKey;

    // A known-good payload + its serialized .lic text.
    LicensePayload m_payload;
    QByteArray m_validLic;
};

namespace {
LicensePayload makeSamplePayload() {
    LicensePayload p;
    p.licensee = QStringLiteral("Test Sponsor");
    p.email = QStringLiteral("test-sponsor@example.com");
    p.issuedAt = QStringLiteral("2026-07-19T00:00:00Z");
    p.tags = QStringList{ QStringLiteral("sponsor"), QStringLiteral("test") };
    return p;
}
} // namespace

void LicenseVerifierTest::initTestCase()
{
    // Fresh keypair each run — the test verifies against THIS key, not the
    // one embedded in the application.
    const QByteArray seed = LicenseCrypto::generateRandomSeed();
    m_publicKey = LicenseCrypto::derivePublicKey(seed);
    m_secretKey = LicenseCrypto::deriveSecretKey(seed);

    QCOMPARE(m_publicKey.size(), LicenseCrypto::kPublicKeySize);
    QCOMPARE(m_secretKey.size(), LicenseCrypto::kSecretKeySize);

    m_payload = makeSamplePayload();
    m_validLic = buildLic(m_payload);
    QVERIFY(!m_validLic.isEmpty());
}

QByteArray LicenseVerifierTest::buildLic(const LicensePayload& p) const
{
    const QByteArray canonical = LicenseFile::canonicalBytes(p);
    const QByteArray sig = LicenseCrypto::sign(m_secretKey, canonical);

    QJsonObject payloadObj;
    payloadObj[u"email"_s]     = p.email;
    payloadObj[u"issued_at"_s] = p.issuedAt;
    payloadObj[u"licensee"_s]  = p.licensee;
    QJsonArray tagsArr;
    for (const QString& t : p.tags)
        tagsArr.append(t);
    payloadObj[u"tags"_s] = tagsArr;

    QJsonObject root;
    root[u"payload"_s] = payloadObj;
    root[u"sig"_s] = QString::fromLatin1(sig.toBase64());

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

void LicenseVerifierTest::validLicense_verifies()
{
    QVERIFY(LicenseVerifier::verifyContent(m_validLic, m_publicKey));
}

void LicenseVerifierTest::validLicense_fillsPayload()
{
    LicensePayload out;
    QVERIFY(LicenseVerifier::verifyContent(m_validLic, m_publicKey, &out));
    QCOMPARE(out.licensee,  m_payload.licensee);
    QCOMPARE(out.email,     m_payload.email);
    QCOMPARE(out.issuedAt,  m_payload.issuedAt);
    QCOMPARE(out.tags,      m_payload.tags);
}

void LicenseVerifierTest::tamperedLicensee_rejected()
{
    // Parse-modify-reserialize: the signature no longer matches the payload.
    QJsonDocument doc = QJsonDocument::fromJson(m_validLic);
    QJsonObject root = doc.object();
    QJsonObject payloadObj = root.value(u"payload"_s).toObject();
    payloadObj[u"licensee"_s] = u"Evil"_s;
    root[u"payload"_s] = payloadObj;
    const QByteArray tampered = QJsonDocument(root).toJson();
    QVERIFY(!LicenseVerifier::verifyContent(tampered, m_publicKey));
}

void LicenseVerifierTest::tamperedSignature_rejected()
{
    // Flip one character of the base64 signature.
    QJsonDocument doc = QJsonDocument::fromJson(m_validLic);
    QJsonObject root = doc.object();
    QString sig = root.value(u"sig"_s).toString();
    sig.replace(0, 1, sig.at(0) == QLatin1Char('A') ? u'B' : u'A');
    root[u"sig"_s] = sig;
    const QByteArray tampered = QJsonDocument(root).toJson();
    QVERIFY(!LicenseVerifier::verifyContent(tampered, m_publicKey));
}

void LicenseVerifierTest::emptyContent_rejected()
{
    QVERIFY(!LicenseVerifier::verifyContent(QByteArray(), m_publicKey));
    QVERIFY(!LicenseVerifier::verifyContent(QString(), m_publicKey));
}

void LicenseVerifierTest::invalidJson_rejected()
{
    QVERIFY(!LicenseVerifier::verifyContent(QByteArray("not json"), m_publicKey));
}

void LicenseVerifierTest::missingPayload_rejected()
{
    const QByteArray noPayload = R"({"sig": "AAAA"})";
    QVERIFY(!LicenseVerifier::verifyContent(noPayload, m_publicKey));
}

QTEST_MAIN(LicenseVerifierTest)
#include "license_verifier_test.moc"
