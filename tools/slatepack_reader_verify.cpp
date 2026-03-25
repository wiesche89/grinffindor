#include <QByteArray>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <iostream>

#include "../src/wallet/binaryslatev4reader.h"

namespace {

bool expect(bool condition, const QString &message)
{
    if (!condition) {
        std::cerr << message.toStdString() << std::endl;
        return false;
    }
    return true;
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

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const bool ok = verifyJsonSlatepackRoundTrip()
        && verifyEncryptedRecognition()
        && verifyInvalidPayloadFailure();

    if (!ok) {
        return 1;
    }

    std::cout << "Slatepack reader verification passed." << std::endl;
    return 0;
}
