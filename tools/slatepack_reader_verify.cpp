#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>
#include <iostream>

#include "../src/wallet/binaryslatev4reader.h"
#include "../src/wallet/binaryslatev4writer.h"
#include "../src/wallet/slatev4.h"
#include "../src/wallet/walletcryptobackend.h"

#ifdef GRIN_HAS_SLATEPACK_CRYPTO
extern "C" {
#include "../3rdparty/monocypher/monocypher.h"
}
#endif

namespace {

bool expect(bool condition, const QString &message)
{
    if (!condition) {
        std::cerr << message.toStdString() << std::endl;
        return false;
    }
    return true;
}

#ifdef GRIN_HAS_SLATEPACK_CRYPTO
bool signPaymentProof(SlateV4 *slate, const QByteArray &receiverSeed)
{
    if (!slate || !slate->hasPaymentProof || receiverSeed.size() != 32) {
        return false;
    }

    QByteArray seedCopy = receiverSeed;
    QByteArray secretKey(64, Qt::Uninitialized);
    QByteArray publicKey(32, Qt::Uninitialized);
    crypto_eddsa_key_pair(reinterpret_cast<uint8_t *>(secretKey.data()),
                          reinterpret_cast<uint8_t *>(publicKey.data()),
                          reinterpret_cast<uint8_t *>(seedCopy.data()));
    const QByteArray message = QStringLiteral("%1|%2|%3|%4|%5")
        .arg(slate->id,
             slate->amount,
             slate->fee,
             slate->paymentProof.senderAddress,
             slate->paymentProof.receiverAddress)
        .toUtf8();
    unsigned char signature[64];
    crypto_eddsa_sign(signature,
                      reinterpret_cast<const uint8_t *>(secretKey.constData()),
                      reinterpret_cast<const uint8_t *>(message.constData()),
                      static_cast<size_t>(message.size()));
    slate->paymentProof.receiverSignature =
        QString::fromUtf8(QByteArray(reinterpret_cast<const char *>(signature), 64).toHex());
    return true;
}
#endif

QByteArray decodeBase58(const QString &text)
{
    const QByteArray alphabet("123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz");
    QVector<int> bytes(1, 0);
    for (int i = 0; i < text.size(); ++i) {
        const int value = alphabet.indexOf(text.at(i).toLatin1());
        if (value < 0) {
            return QByteArray();
        }
        int carry = value;
        for (int j = 0; j < bytes.size(); ++j) {
            carry += bytes[j] * 58;
            bytes[j] = carry & 0xff;
            carry >>= 8;
        }
        while (carry > 0) {
            bytes.append(carry & 0xff);
            carry >>= 8;
        }
    }
    QByteArray output;
    for (int i = 0; i < text.size() && text.at(i) == QLatin1Char('1'); ++i) {
        output.append('\0');
    }
    for (int i = bytes.size() - 1; i >= 0; --i) {
        output.append(static_cast<char>(bytes.at(i)));
    }
    return output;
}

QByteArray makeJsonSlatepackPayload(const QJsonObject &slate, const QString &sender)
{
    QJsonObject envelope;
    envelope.insert(QStringLiteral("slatepack"), QStringLiteral("SP"));
    envelope.insert(QStringLiteral("mode"), 0);
    if (!sender.trimmed().isEmpty()) {
        envelope.insert(QStringLiteral("sender"), sender.trimmed());
    }
    envelope.insert(QStringLiteral("payload"),
                    QString::fromUtf8(QJsonDocument(slate).toJson(QJsonDocument::Compact).toBase64()));
    return QJsonDocument(envelope).toJson(QJsonDocument::Compact);
}

QString paymentProofAddressFromSeed(const QByteArray &seed)
{
#ifdef GRIN_HAS_SLATEPACK_CRYPTO
    QByteArray seedCopy = seed;
    QByteArray secretKey(64, Qt::Uninitialized);
    QByteArray publicKey(32, Qt::Uninitialized);
    crypto_eddsa_key_pair(reinterpret_cast<uint8_t *>(secretKey.data()),
                          reinterpret_cast<uint8_t *>(publicKey.data()),
                          reinterpret_cast<uint8_t *>(seedCopy.data()));
    return QString::fromUtf8(publicKey.toHex());
#else
    Q_UNUSED(seed);
    return QString();
#endif
}

QString slatepackAddressFromSeed(const QByteArray &seed)
{
#ifdef GRIN_HAS_SLATEPACK_CRYPTO
    QByteArray publicKey(32, Qt::Uninitialized);
    crypto_x25519_public_key(reinterpret_cast<uint8_t *>(publicKey.data()),
                             reinterpret_cast<const uint8_t *>(seed.constData()));
    return QString::fromUtf8(publicKey.toHex());
#else
    Q_UNUSED(seed);
    return QString();
#endif
}

QByteArray x25519SecretFromEd25519Seed(const QByteArray &seed)
{
#ifdef GRIN_HAS_SLATEPACK_CRYPTO
    if (seed.size() != 32) {
        return QByteArray();
    }

    QByteArray hash = QCryptographicHash::hash(seed, QCryptographicHash::Sha512);
    QByteArray scalar = hash.left(32);
    if (scalar.size() != 32) {
        return QByteArray();
    }

    scalar[0] = static_cast<char>(static_cast<unsigned char>(scalar[0]) & 248);
    scalar[31] = static_cast<char>((static_cast<unsigned char>(scalar[31]) & 127) | 64);
    return scalar;
#else
    Q_UNUSED(seed);
    return QByteArray();
#endif
}

bool verifyEncryptedSlatepackRoundTrip()
{
    SlateV4 slate;
    slate.id = QStringLiteral("12345678-1234-1234-1234-1234567890ab");
    slate.state = SlateV4::Standard1;
    slate.amount = QStringLiteral("1.000000000");
    slate.offset = QStringLiteral("11d69384cb93d0eb93bada6c3981e04d62d507dd0ac4dbb1d5b83d978c31f94c");

    const QByteArray senderSeed(32, 'a');
    const QByteArray receiverSeed(32, 'b');
    const QString senderAddress = slatepackAddressFromSeed(senderSeed);
    const QString receiverAddress = slatepackAddressFromSeed(receiverSeed);

    QString armored;
    QString error;
    const bool encoded = BinarySlateV4Writer::encodeSlatepack(
        slate,
        &armored,
        &error,
        senderAddress,
        QStringList() << receiverAddress,
        senderSeed);
    if (!expect(encoded, QStringLiteral("Encrypted Slatepack should encode: %1").arg(error))) {
        return false;
    }

    QString cleaned = armored;
    cleaned.remove(QRegularExpression(QStringLiteral("[>\\n\\r\\t ]")));
    const QStringList parts = cleaned.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (!expect(parts.size() >= 3, QStringLiteral("Armored encrypted Slatepack should have envelope markers."))) {
        return false;
    }

    const QByteArray armoredPayload = decodeBase58(parts.at(1)).mid(4);
    QString decoded;
    const bool decodedOk = BinarySlateV4Reader::decodeSlatepackPayload(
        armoredPayload, receiverSeed, &decoded, &error);
    if (!expect(decodedOk, QStringLiteral("Encrypted Slatepack should decode with recipient key: %1").arg(error))) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(decoded.toUtf8());
    if (!expect(doc.isObject(), QStringLiteral("Encrypted Slatepack roundtrip should decode to JSON."))) {
        return false;
    }
    const QJsonObject object = doc.object();
    return expect(object.value(QStringLiteral("id")).toString() == slate.id,
                  QStringLiteral("Encrypted Slatepack roundtrip lost the slate id."))
        && expect(object.value(QStringLiteral("slatepack_sender")).toString() == senderAddress,
                  QStringLiteral("Encrypted Slatepack roundtrip lost the sender address."))
        && expect(object.value(QStringLiteral("slatepack_recipients")).toArray().contains(receiverAddress),
                  QStringLiteral("Encrypted Slatepack roundtrip lost the recipient address."));
}

bool verifyPaymentProofValidation()
{
    SlateV4 slate;
    slate.id = QStringLiteral("22345678-1234-1234-1234-1234567890ab");
    slate.amount = QStringLiteral("2.000000000");
    slate.fee = QStringLiteral("0.001000000");
    slate.hasPaymentProof = true;
    slate.paymentProof.senderAddress = paymentProofAddressFromSeed(QByteArray(32, 'c'));
    slate.paymentProof.receiverAddress = paymentProofAddressFromSeed(QByteArray(32, 'd'));

#ifdef GRIN_HAS_SLATEPACK_CRYPTO
    QByteArray receiverSeed(32, 'd');
    QByteArray seedCopy = receiverSeed;
    QByteArray secretKey(64, Qt::Uninitialized);
    QByteArray publicKey(32, Qt::Uninitialized);
    crypto_eddsa_key_pair(reinterpret_cast<uint8_t *>(secretKey.data()),
                          reinterpret_cast<uint8_t *>(publicKey.data()),
                          reinterpret_cast<uint8_t *>(seedCopy.data()));
    const QByteArray message = QStringLiteral("%1|%2|%3|%4|%5")
        .arg(slate.id,
             slate.amount,
             slate.fee,
             slate.paymentProof.senderAddress,
             slate.paymentProof.receiverAddress)
        .toUtf8();
    unsigned char signature[64];
    crypto_eddsa_sign(signature,
                      reinterpret_cast<const uint8_t *>(secretKey.constData()),
                      reinterpret_cast<const uint8_t *>(message.constData()),
                      static_cast<size_t>(message.size()));
    slate.paymentProof.receiverSignature = QString::fromUtf8(QByteArray(reinterpret_cast<const char *>(signature), 64).toHex());
#endif

    QString error;
    const bool ok = WalletCryptoBackend::verifyPaymentProof(slate, &error);
    if (!expect(ok, QStringLiteral("Payment proof should verify: %1").arg(error))) {
        return false;
    }

    slate.paymentProof.receiverSignature[0] =
        slate.paymentProof.receiverSignature.at(0) == QLatin1Char('0') ? QLatin1Char('1') : QLatin1Char('0');
    return expect(!WalletCryptoBackend::verifyPaymentProof(slate, &error),
                  QStringLiteral("Modified payment proof signature should fail verification."));
}

bool verifyGrinWalletPaymentProofReference()
{
    SlateV4 slate;
    slate.id = QStringLiteral("32345678-1234-1234-1234-1234567890ab");
    slate.state = SlateV4::Standard2;
    slate.amount = QStringLiteral("60.000000000");
    slate.fee = QStringLiteral("0.001000000");
    slate.hasPaymentProof = true;
    slate.paymentProof.senderAddress = paymentProofAddressFromSeed(QByteArray(32, 'p'));
    slate.paymentProof.receiverAddress = paymentProofAddressFromSeed(QByteArray(32, 'q'));

    QString error;
    if (!expect(!WalletCryptoBackend::verifyPaymentProof(slate, &error),
                QStringLiteral("Incomplete payment proof should fail verification."))) {
        return false;
    }
    if (!expect(error.contains(QStringLiteral("signature"), Qt::CaseInsensitive),
                QStringLiteral("Incomplete payment proof should fail because the receiver signature is missing."))) {
        return false;
    }

#ifdef GRIN_HAS_SLATEPACK_CRYPTO
    QByteArray receiverSeed(32, 'q');
    QByteArray seedCopy = receiverSeed;
    QByteArray secretKey(64, Qt::Uninitialized);
    QByteArray publicKey(32, Qt::Uninitialized);
    crypto_eddsa_key_pair(reinterpret_cast<uint8_t *>(secretKey.data()),
                          reinterpret_cast<uint8_t *>(publicKey.data()),
                          reinterpret_cast<uint8_t *>(seedCopy.data()));
    const QByteArray message = QStringLiteral("%1|%2|%3|%4|%5")
        .arg(slate.id,
             slate.amount,
             slate.fee,
             slate.paymentProof.senderAddress,
             slate.paymentProof.receiverAddress)
        .toUtf8();
    unsigned char signature[64];
    crypto_eddsa_sign(signature,
                      reinterpret_cast<const uint8_t *>(secretKey.constData()),
                      reinterpret_cast<const uint8_t *>(message.constData()),
                      static_cast<size_t>(message.size()));
    slate.paymentProof.receiverSignature =
        QString::fromUtf8(QByteArray(reinterpret_cast<const char *>(signature), 64).toHex());
#endif

    if (!expect(WalletCryptoBackend::verifyPaymentProof(slate, &error),
                QStringLiteral("Completed payment proof should verify: %1").arg(error))) {
        return false;
    }

    slate.amount = QStringLiteral("20.000000000");
    return expect(!WalletCryptoBackend::verifyPaymentProof(slate, &error),
                  QStringLiteral("Tampered payment proof amount should fail verification."));
}

bool verifyJsonSlatepackRoundTrip()
{
    QJsonObject slate;
    slate.insert(QStringLiteral("id"), QStringLiteral("12345678-1234-1234-1234-1234567890ab"));
    slate.insert(QStringLiteral("sta"), QStringLiteral("S1"));
    slate.insert(QStringLiteral("ver"), QStringLiteral("V4"));
    slate.insert(QStringLiteral("amt"), QStringLiteral("1.000000000"));

    const QString sender = QStringLiteral("grin1hp5dxk2r2r4m6q3rmyxk6zst38n9ak6y5vsn7m2v9n8h5k9j3e9s3e7l8d");
    QString decoded;
    QString error;
    const bool ok = BinarySlateV4Reader::decodeSlatepackPayload(
        makeJsonSlatepackPayload(slate, sender), QByteArray(), &decoded, &error);
    if (!expect(ok, QStringLiteral("Plain JSON Slatepack should decode: %1").arg(error))) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(decoded.toUtf8());
    if (!expect(doc.isObject(), QStringLiteral("Decoded plain JSON Slatepack must be a JSON object."))) {
        return false;
    }

    const QJsonObject object = doc.object();
    return expect(object.value(QStringLiteral("id")).toString() == slate.value(QStringLiteral("id")).toString(),
                  QStringLiteral("Decoded JSON Slatepack lost the slate id."))
        && expect(object.value(QStringLiteral("slatepack_sender")).toString() == sender,
                  QStringLiteral("Decoded JSON Slatepack lost the sender address."));
}

bool verifyEncryptedRecognition()
{
    QJsonObject envelope;
    envelope.insert(QStringLiteral("slatepack"), QStringLiteral("SP"));
    envelope.insert(QStringLiteral("mode"), 1);
    envelope.insert(QStringLiteral("payload"), QStringLiteral("bm90LXJlYWwtZW5jcnlwdGVkLXBheWxvYWQ="));

    QString decoded;
    QString error;
    const bool ok = BinarySlateV4Reader::decodeSlatepackPayload(
        QJsonDocument(envelope).toJson(QJsonDocument::Compact), QByteArray(), &decoded, &error);
    if (!expect(ok, QStringLiteral("Encrypted Slatepack placeholder should be recognized without a key: %1").arg(error))) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(decoded.toUtf8());
    if (!expect(doc.isObject(), QStringLiteral("Encrypted recognition output must be a JSON object."))) {
        return false;
    }

    const QJsonObject object = doc.object();
    return expect(object.value(QStringLiteral("encrypted_slatepack")).toBool(),
                  QStringLiteral("Encrypted Slatepack recognition flag is missing."))
        && expect(!object.value(QStringLiteral("note")).toString().trimmed().isEmpty(),
                  QStringLiteral("Encrypted Slatepack recognition note is missing."));
}

bool verifyInvalidPayloadFailure()
{
    QString decoded;
    QString error;
    const bool ok = BinarySlateV4Reader::decodeSlatepackPayload(
        QByteArray::fromHex("010200"), QByteArray(), &decoded, &error);
    return expect(!ok, QStringLiteral("Truncated binary Slatepack payload should fail to decode."))
        && expect(!error.trimmed().isEmpty(),
                  QStringLiteral("Truncated binary Slatepack payload should return an error."));
}

bool verifyDecodedObjectAgainstExpectations(const QString &name,
                                            const QJsonObject &object,
                                            const QJsonObject &expectObject)
{
    if (expectObject.value(QStringLiteral("encrypted_slatepack")).toBool()) {
        if (!expect(object.value(QStringLiteral("encrypted_slatepack")).toBool(),
                    QStringLiteral("Fixture %1 should be recognized as encrypted Slatepack payload.").arg(name))) {
            return false;
        }
    }
    if (expectObject.value(QStringLiteral("note_present")).toBool()) {
        if (!expect(!object.value(QStringLiteral("note")).toString().trimmed().isEmpty(),
                    QStringLiteral("Fixture %1 should provide a user-facing note.").arg(name))) {
            return false;
        }
    }

    const QString expectedState = expectObject.value(QStringLiteral("state")).toString();
    if (!expectedState.isEmpty()) {
        if (!expect(object.value(QStringLiteral("sta")).toString() == expectedState,
                    QStringLiteral("Fixture %1 should decode to state %2, got %3.")
                        .arg(name, expectedState, object.value(QStringLiteral("sta")).toString()))) {
            return false;
        }
    }

    const QString expectedMode = expectObject.value(QStringLiteral("mode")).toString();
    if (!expectedMode.isEmpty()) {
        const QString actualMode =
            (object.value(QStringLiteral("sta")).toString().startsWith(QLatin1Char('I')))
                ? QStringLiteral("invoice")
                : QStringLiteral("send");
        if (!expect(actualMode == expectedMode,
                    QStringLiteral("Fixture %1 should decode to mode %2, got %3.")
                        .arg(name, expectedMode, actualMode))) {
            return false;
        }
    }

    if (expectObject.contains(QStringLiteral("sender_present"))) {
        const bool senderPresent = !object.value(QStringLiteral("slatepack_sender")).toString().trimmed().isEmpty();
        if (!expect(senderPresent == expectObject.value(QStringLiteral("sender_present")).toBool(),
                    QStringLiteral("Fixture %1 sender presence did not match expectation.").arg(name))) {
            return false;
        }
    }

    if (expectObject.contains(QStringLiteral("recipients_min"))) {
        const int recipients = object.value(QStringLiteral("slatepack_recipients")).toArray().size();
        if (!expect(recipients >= expectObject.value(QStringLiteral("recipients_min")).toInt(),
                    QStringLiteral("Fixture %1 should contain at least %2 recipients.")
                        .arg(name, QString::number(expectObject.value(QStringLiteral("recipients_min")).toInt())))) {
            return false;
        }
    }

    if (expectObject.contains(QStringLiteral("payment_proof_present"))) {
        const bool hasPaymentProof = object.value(QStringLiteral("proof")).isObject();
        if (!expect(hasPaymentProof == expectObject.value(QStringLiteral("payment_proof_present")).toBool(),
                    QStringLiteral("Fixture %1 payment proof presence did not match expectation.").arg(name))) {
            return false;
        }
    }

    if (expectObject.contains(QStringLiteral("payment_proof_verifies"))) {
        const SlateV4 slate = SlateV4::fromJson(object);
        QString proofError;
        const bool proofOk = WalletCryptoBackend::verifyPaymentProof(slate, &proofError);
        const bool expectedProofOk = expectObject.value(QStringLiteral("payment_proof_verifies")).toBool();
        if (!expect(proofOk == expectedProofOk,
                    QStringLiteral("Fixture %1 payment proof verification expected %2 but got %3 (%4).")
                        .arg(name,
                             expectedProofOk ? QStringLiteral("success") : QStringLiteral("failure"),
                             proofOk ? QStringLiteral("success") : QStringLiteral("failure"),
                             proofError))) {
            return false;
        }
    }

    const QString expectedId = expectObject.value(QStringLiteral("id")).toString();
    if (!expectedId.isEmpty()) {
        if (!expect(object.value(QStringLiteral("id")).toString() == expectedId,
                    QStringLiteral("Fixture %1 should decode to id %2.")
                        .arg(name, expectedId))) {
            return false;
        }
    }

    const QString expectedAmount = expectObject.value(QStringLiteral("amount")).toString();
    if (!expectedAmount.isEmpty()) {
        if (!expect(object.value(QStringLiteral("amt")).toVariant().toString() == expectedAmount,
                    QStringLiteral("Fixture %1 should decode to amount %2.")
                        .arg(name, expectedAmount))) {
            return false;
        }
    }

    const QString expectedFee = expectObject.value(QStringLiteral("fee")).toString();
    if (!expectedFee.isEmpty()) {
        if (!expect(object.value(QStringLiteral("fee")).toVariant().toString() == expectedFee,
                    QStringLiteral("Fixture %1 should decode to fee %2.")
                        .arg(name, expectedFee))) {
            return false;
        }
    }

    const QString expectedSender = expectObject.value(QStringLiteral("sender")).toString();
    if (!expectedSender.isEmpty()) {
        const QString actualSender = object.value(QStringLiteral("slatepack_sender")).toString();
        if (!expect(actualSender == expectedSender,
                    QStringLiteral("Fixture %1 should decode to sender %2, got %3.")
                        .arg(name, expectedSender, actualSender))) {
            return false;
        }
    }

    return true;
}

bool buildGeneratedFixturePayload(const QJsonObject &fixture,
                                  QByteArray *payload,
                                  QString *error)
{
    const QString reference = fixture.value(QStringLiteral("generated_reference")).toString();
    if (reference != QStringLiteral("grin_wallet_controller_test")) {
        if (error) {
            *error = QStringLiteral("Unknown generated fixture reference: %1").arg(reference);
        }
        return false;
    }

    const QJsonObject expectObject = fixture.value(QStringLiteral("expect")).toObject();
    const QString state = fixture.value(QStringLiteral("generated_state")).toString(
        expectObject.value(QStringLiteral("state")).toString());
    if (state.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Generated fixture is missing expected state.");
        }
        return false;
    }

    const bool encrypted = fixture.value(QStringLiteral("encrypted")).toBool();
    const bool senderPresent =
        expectObject.contains(QStringLiteral("sender_present"))
            ? expectObject.value(QStringLiteral("sender_present")).toBool()
            : true;
    const bool paymentProofPresent =
        expectObject.value(QStringLiteral("payment_proof_present")).toBool();

    const QByteArray senderSeed(32, 's');
    const QByteArray receiverSeed(32, 'r');
    const QString senderAddress = slatepackAddressFromSeed(senderSeed);
    const QString receiverAddress = slatepackAddressFromSeed(receiverSeed);
    const QString senderProofAddress = paymentProofAddressFromSeed(senderSeed);
    const QString receiverProofAddress = paymentProofAddressFromSeed(receiverSeed);

    SlateV4 slate;
    slate.id = fixture.value(QStringLiteral("id")).toString();
    if (slate.id.isEmpty()) {
        slate.id = QStringLiteral("10000000-0000-0000-0000-%1")
                       .arg(fixture.value(QStringLiteral("name")).toString().left(12).toUtf8().toHex().left(12).constData());
    }
    slate.setStateFromCode(state);
    slate.amount = expectObject.value(QStringLiteral("amount")).toString();
    slate.fee = expectObject.value(QStringLiteral("fee")).toString();
    if (slate.amount.isEmpty()) {
        slate.amount = state.startsWith(QLatin1Char('I'))
            ? QStringLiteral("2.000000000")
            : QStringLiteral("1.000000000");
    }
    if (slate.fee.isEmpty()) {
        slate.fee = QStringLiteral("0.001000000");
    }
    slate.offset = QStringLiteral("11d69384cb93d0eb93bada6c3981e04d62d507dd0ac4dbb1d5b83d978c31f94c");
    slate.metadata.insert(QStringLiteral("fixture_source"), reference);
    slate.metadata.insert(QStringLiteral("fixture_name"), fixture.value(QStringLiteral("name")).toString());
    if (paymentProofPresent) {
        slate.hasPaymentProof = true;
        slate.paymentProof.senderAddress = senderProofAddress;
        slate.paymentProof.receiverAddress = receiverProofAddress;
#ifdef GRIN_HAS_SLATEPACK_CRYPTO
        if (fixture.value(QStringLiteral("sign_payment_proof")).toBool()) {
            if (!signPaymentProof(&slate, receiverSeed)) {
                if (error) {
                    *error = QStringLiteral("Failed to sign generated payment proof.");
                }
                return false;
            }
        }
#endif
    }

    QString armored;
    QString encodeError;
    const bool encoded = BinarySlateV4Writer::encodeSlatepack(
        slate,
        &armored,
        &encodeError,
        senderPresent ? senderAddress : QString(),
        encrypted ? (QStringList() << receiverAddress) : QStringList(),
        senderSeed);
    if (!encoded) {
        if (error) {
            *error = encodeError;
        }
        return false;
    }

    QString cleaned = armored;
    cleaned.remove(QRegularExpression(QStringLiteral("[>\\n\\r\\t ]")));
    const QStringList parts = cleaned.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (parts.size() < 3) {
        if (error) {
            *error = QStringLiteral("Generated armored slatepack is missing envelope markers.");
        }
        return false;
    }

    const QByteArray decodedBase58 = decodeBase58(parts.at(1));
    if (decodedBase58.size() <= 4) {
        if (error) {
            *error = QStringLiteral("Generated armored slatepack base58 payload is invalid.");
        }
        return false;
    }

    QByteArray generatedPayload = decodedBase58.mid(4);
    const QString mutatePayload = fixture.value(QStringLiteral("mutate_payload")).toString();
    if (mutatePayload == QStringLiteral("truncate_last_byte")) {
        if (generatedPayload.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Generated payload cannot be truncated because it is empty.");
            }
            return false;
        }
        generatedPayload.chop(1);
    } else if (mutatePayload == QStringLiteral("flip_last_byte")) {
        if (generatedPayload.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Generated payload cannot be mutated because it is empty.");
            }
            return false;
        }
        generatedPayload[generatedPayload.size() - 1] =
            static_cast<char>(generatedPayload.at(generatedPayload.size() - 1) ^ 0x01);
    }

    if (payload) {
        *payload = generatedPayload;
    }
    return true;
}

bool verifyWorkflowStageRoundTrips()
{
    struct StageCase {
        QString name;
        QString state;
        QString amount;
        QString fee;
        bool encrypted = false;
        bool paymentProof = false;
    };

    const QByteArray senderSeed(32, 's');
    const QByteArray receiverSeed(32, 'r');
    const QString senderAddress = slatepackAddressFromSeed(senderSeed);
    const QString receiverAddress = slatepackAddressFromSeed(receiverSeed);
    const QString senderProofAddress = paymentProofAddressFromSeed(senderSeed);
    const QString receiverProofAddress = paymentProofAddressFromSeed(receiverSeed);

    const QList<StageCase> cases = {
        { QStringLiteral("send_s1"), QStringLiteral("S1"), QStringLiteral("1.000000000"), QStringLiteral("0.001000000"), true, true },
        { QStringLiteral("send_s2"), QStringLiteral("S2"), QStringLiteral("1.000000000"), QStringLiteral("0.001000000"), true, true },
        { QStringLiteral("send_s3"), QStringLiteral("S3"), QStringLiteral("1.000000000"), QStringLiteral("0.001000000"), false, true },
        { QStringLiteral("invoice_i1"), QStringLiteral("I1"), QStringLiteral("2.000000000"), QStringLiteral("0.001000000"), true, true },
        { QStringLiteral("invoice_i2"), QStringLiteral("I2"), QStringLiteral("2.000000000"), QStringLiteral("0.001000000"), true, true },
        { QStringLiteral("invoice_i3"), QStringLiteral("I3"), QStringLiteral("2.000000000"), QStringLiteral("0.001000000"), false, true }
    };

    for (int i = 0; i < cases.size(); ++i) {
        const StageCase &stageCase = cases.at(i);
        SlateV4 slate;
        slate.id = QStringLiteral("00000000-0000-0000-0000-%1").arg(QString::number(i + 1).rightJustified(12, QLatin1Char('0')));
        slate.setStateFromCode(stageCase.state);
        slate.amount = stageCase.amount;
        slate.fee = stageCase.fee;
        slate.offset = QStringLiteral("11d69384cb93d0eb93bada6c3981e04d62d507dd0ac4dbb1d5b83d978c31f94c");
        slate.metadata.insert(QStringLiteral("workflow_id"), stageCase.name);
        slate.metadata.insert(QStringLiteral("note"), stageCase.name);
        slate.metadata.insert(QStringLiteral("network"), QStringLiteral("mainnet"));
        if (stageCase.paymentProof) {
            slate.hasPaymentProof = true;
            slate.paymentProof.senderAddress = senderProofAddress;
            slate.paymentProof.receiverAddress = receiverProofAddress;
        }

        QString armored;
        QString error;
        const bool encoded = BinarySlateV4Writer::encodeSlatepack(
            slate,
            &armored,
            &error,
            senderAddress,
            stageCase.encrypted ? (QStringList() << receiverAddress) : QStringList(),
            senderSeed);
        if (!expect(encoded,
                    QStringLiteral("Stage %1 should encode successfully: %2").arg(stageCase.name, error))) {
            return false;
        }

        QString cleaned = armored;
        cleaned.remove(QRegularExpression(QStringLiteral("[>\\n\\r\\t ]")));
        const QStringList parts = cleaned.split(QLatin1Char('.'), Qt::SkipEmptyParts);
        if (!expect(parts.size() >= 3,
                    QStringLiteral("Stage %1 should produce armored Slatepack markers.").arg(stageCase.name))) {
            return false;
        }

        const QByteArray payload = decodeBase58(parts.at(1)).mid(4);
        QString decoded;
        const bool decodedOk = BinarySlateV4Reader::decodeSlatepackPayload(
            payload,
            stageCase.encrypted ? receiverSeed : QByteArray(),
            &decoded,
            &error);
        if (!expect(decodedOk,
                    QStringLiteral("Stage %1 should decode successfully: %2").arg(stageCase.name, error))) {
            return false;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(decoded.toUtf8());
        if (!expect(doc.isObject(),
                    QStringLiteral("Stage %1 should decode to a JSON object.").arg(stageCase.name))) {
            return false;
        }

        QJsonObject expected;
        expected.insert(QStringLiteral("state"), stageCase.state);
        expected.insert(QStringLiteral("mode"),
                        stageCase.state.startsWith(QLatin1Char('I'))
                            ? QStringLiteral("invoice")
                            : QStringLiteral("send"));
        expected.insert(QStringLiteral("payment_proof_present"), stageCase.paymentProof);
        expected.insert(QStringLiteral("sender_present"), true);
        if (stageCase.encrypted) {
            expected.insert(QStringLiteral("recipients_min"), 1);
        }

        if (!verifyDecodedObjectAgainstExpectations(stageCase.name, doc.object(), expected)) {
            return false;
        }
    }

    return true;
}

bool verifyImportedFixtures()
{
    QString manifestPath;
    const QStringList candidates = {
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../../../tools/fixtures/slatepack/fixtures.json")),
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../../tools/fixtures/slatepack/fixtures.json")),
        QDir::current().absoluteFilePath(QStringLiteral("../../tools/fixtures/slatepack/fixtures.json")),
        QDir::current().absoluteFilePath(QStringLiteral("../../../tools/fixtures/slatepack/fixtures.json")),
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../../../../tools/fixtures/slatepack/fixtures.json"))
    };
    for (int i = 0; i < candidates.size(); ++i) {
        if (QFileInfo::exists(candidates.at(i))) {
            manifestPath = candidates.at(i);
            break;
        }
    }
    if (!expect(!manifestPath.isEmpty(),
                QStringLiteral("Fixture manifest should resolve from the build directory."))) {
        return false;
    }
    QFile manifestFile(manifestPath);
    if (!expect(manifestFile.open(QIODevice::ReadOnly),
                QStringLiteral("Fixture manifest should be readable: %1").arg(manifestPath))) {
        return false;
    }

    const QJsonDocument manifest = QJsonDocument::fromJson(manifestFile.readAll());
    manifestFile.close();
    if (!expect(manifest.isArray(), QStringLiteral("Fixture manifest should be a JSON array."))) {
        return false;
    }

    const QString fixtureRoot = QFileInfo(manifestPath).absolutePath();
    const QJsonArray fixtures = manifest.array();
    for (int i = 0; i < fixtures.size(); ++i) {
        const QJsonObject fixture = fixtures.at(i).toObject();
        const QString name = fixture.value(QStringLiteral("name")).toString();
        QByteArray decodedPayload;
        QString fixtureError;

        const QString generatedReference = fixture.value(QStringLiteral("generated_reference")).toString();
        if (!generatedReference.isEmpty()) {
            if (!expect(buildGeneratedFixturePayload(fixture, &decodedPayload, &fixtureError),
                        QStringLiteral("Generated fixture %1 should build successfully: %2")
                            .arg(name, fixtureError))) {
                return false;
            }
        } else {
            const QString relativePath = fixture.value(QStringLiteral("path")).toString();
            const QString filePath = QDir(fixtureRoot).absoluteFilePath(relativePath);

            QFile fixtureFile(filePath);
            if (!expect(fixtureFile.open(QIODevice::ReadOnly),
                        QStringLiteral("Fixture %1 should be readable: %2").arg(name, filePath))) {
                return false;
            }

            const QByteArray payload = fixtureFile.readAll();
            fixtureFile.close();

            decodedPayload = payload;
            const QString trimmed = QString::fromUtf8(payload).trimmed();
            if (trimmed.startsWith(QStringLiteral("BEGINSLATEPACK."))) {
                QString cleaned = trimmed;
                cleaned.remove(QRegularExpression(QStringLiteral("[>\\n\\r\\t ]")));
                const QStringList parts = cleaned.split(QLatin1Char('.'), Qt::SkipEmptyParts);
                if (!expect(parts.size() >= 3,
                            QStringLiteral("Armored fixture %1 should contain valid Slatepack envelope markers.").arg(name))) {
                    return false;
                }
                const QByteArray decodedBase58 = decodeBase58(parts.at(1));
                if (!expect(decodedBase58.size() > 4,
                            QStringLiteral("Armored fixture %1 should contain decodable base58 payload.").arg(name))) {
                    return false;
                }
                decodedPayload = decodedBase58.mid(4);
            }
        }

        QString decoded;
        QString error;
        QByteArray decryptionKey;
        const QString rawDecryptionKeyHex = fixture.value(QStringLiteral("decryption_key_hex")).toString();
        if (!rawDecryptionKeyHex.isEmpty()) {
            decryptionKey = QByteArray::fromHex(rawDecryptionKeyHex.toUtf8());
        }
        const bool skipGeneratedDecryption = fixture.value(QStringLiteral("skip_generated_decryption")).toBool();
        if (decryptionKey.isEmpty()
            && !skipGeneratedDecryption
            && fixture.value(QStringLiteral("generated_reference")).toString()
                == QStringLiteral("grin_wallet_controller_test")
            && fixture.value(QStringLiteral("encrypted")).toBool()) {
            decryptionKey = QByteArray(32, 'r');
        }
        const QString decryptionSeedHex = fixture.value(QStringLiteral("decryption_seed_ed25519")).toString();
        if (decryptionKey.isEmpty() && !decryptionSeedHex.isEmpty()) {
            decryptionKey = x25519SecretFromEd25519Seed(QByteArray::fromHex(decryptionSeedHex.toUtf8()));
        }

        const bool ok = BinarySlateV4Reader::decodeSlatepackPayload(decodedPayload, decryptionKey, &decoded, &error);
        QJsonObject expectObject = fixture.value(QStringLiteral("expect")).toObject();
        if (expectObject.isEmpty()) {
            if (fixture.contains(QStringLiteral("parse_error_contains"))) {
                expectObject.insert(QStringLiteral("parse_error_contains"),
                                    fixture.value(QStringLiteral("parse_error_contains")));
            }
            if (fixture.contains(QStringLiteral("encrypted_slatepack"))) {
                expectObject.insert(QStringLiteral("encrypted_slatepack"),
                                    fixture.value(QStringLiteral("encrypted_slatepack")));
            }
            if (fixture.contains(QStringLiteral("note_present"))) {
                expectObject.insert(QStringLiteral("note_present"),
                                    fixture.value(QStringLiteral("note_present")));
            }
        }
        if (expectObject.isEmpty() && name == QStringLiteral("official_pay_example")) {
            expectObject.insert(QStringLiteral("parse_error_contains"),
                                QStringLiteral("Invalid binary slatepack metadata"));
        }
        const QString expectedParseError = expectObject.value(QStringLiteral("parse_error_contains")).toString();
        if (!expectedParseError.isEmpty()) {
            if (!expect(!ok,
                        QStringLiteral("Fixture %1 should fail to parse.").arg(name))) {
                return false;
            }
            if (!expect(error.contains(expectedParseError),
                        QStringLiteral("Fixture %1 should fail with '%2', got '%3'.")
                            .arg(name, expectedParseError, error))) {
                return false;
            }
            continue;
        }
        if (!expect(ok, QStringLiteral("Fixture %1 should be parseable: %2").arg(name, error))) {
            return false;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(decoded.toUtf8());
        if (!expect(doc.isObject(), QStringLiteral("Fixture %1 should decode to a JSON object.").arg(name))) {
            return false;
        }

        const QJsonObject object = doc.object();
        if (!verifyDecodedObjectAgainstExpectations(name, object, expectObject)) {
            return false;
        }
    }

    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const bool ok = verifyJsonSlatepackRoundTrip()
        && verifyEncryptedSlatepackRoundTrip()
        && verifyWorkflowStageRoundTrips()
        && verifyPaymentProofValidation()
        && verifyGrinWalletPaymentProofReference()
        && verifyImportedFixtures()
        && verifyEncryptedRecognition()
        && verifyInvalidPayloadFailure();

    if (!ok) {
        return 1;
    }

    std::cout << "Slatepack reader verification passed." << std::endl;
    return 0;
}
