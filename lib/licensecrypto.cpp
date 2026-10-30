#include "licensecrypto.h"

#include <QRandomGenerator>

#include <algorithm>   // std::copy_n
#include <cstdint>     // uint8_t

extern "C" {
#include <monocypher-ed25519.h>
}

namespace LicenseCrypto {

namespace {

bool isSize(const QByteArray& b, int expected) {
    return b.size() == expected;
}

} // namespace

QByteArray generateRandomSeed() {
    QByteArray seed(kSeedSize, '\0');
    auto* rng = QRandomGenerator::system();
    for (int i = 0; i < kSeedSize; ++i) {
        seed[i] = static_cast<char>(rng->generate() & 0xFF);
    }
    return seed;
}

QByteArray derivePublicKey(const QByteArray& seed) {
    if (!isSize(seed, kSeedSize)) return {};
    // crypto_ed25519_key_pair() wipes its seed argument (security measure),
    // so we operate on a private mutable copy.
    uint8_t seedBuf[kSeedSize];
    std::copy_n(reinterpret_cast<const uint8_t*>(seed.constData()), kSeedSize, seedBuf);
    QByteArray sk(kSecretKeySize, '\0');
    QByteArray pk(kPublicKeySize, '\0');
    crypto_ed25519_key_pair(reinterpret_cast<uint8_t*>(sk.data()),
                            reinterpret_cast<uint8_t*>(pk.data()),
                            seedBuf);
    return pk;
}

QByteArray deriveSecretKey(const QByteArray& seed) {
    if (!isSize(seed, kSeedSize)) return {};
    uint8_t seedBuf[kSeedSize];
    std::copy_n(reinterpret_cast<const uint8_t*>(seed.constData()), kSeedSize, seedBuf);
    QByteArray sk(kSecretKeySize, '\0');
    QByteArray pk(kPublicKeySize, '\0');
    crypto_ed25519_key_pair(reinterpret_cast<uint8_t*>(sk.data()),
                            reinterpret_cast<uint8_t*>(pk.data()),
                            seedBuf);
    return sk;
}

QByteArray sign(const QByteArray& secretKey, const QByteArray& message) {
    if (!isSize(secretKey, kSecretKeySize)) return {};
    QByteArray sig(kSignatureSize, '\0');
    crypto_ed25519_sign(reinterpret_cast<uint8_t*>(sig.data()),
                        reinterpret_cast<const uint8_t*>(secretKey.constData()),
                        reinterpret_cast<const uint8_t*>(message.constData()),
                        static_cast<size_t>(message.size()));
    return sig;
}

bool verify(const QByteArray& signature,
            const QByteArray& publicKey,
            const QByteArray& message) {
    if (!isSize(signature, kSignatureSize)) return false;
    if (!isSize(publicKey, kPublicKeySize)) return false;
    int rc = crypto_ed25519_check(reinterpret_cast<const uint8_t*>(signature.constData()),
                                  reinterpret_cast<const uint8_t*>(publicKey.constData()),
                                  reinterpret_cast<const uint8_t*>(message.constData()),
                                  static_cast<size_t>(message.size()));
    return rc == 0;
}

} // namespace LicenseCrypto
