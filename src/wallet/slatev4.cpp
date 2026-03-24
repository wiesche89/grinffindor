#include "slatev4.h"

#include <QJsonDocument>
#include <QUuid>

QJsonObject SlateV4::ParticipantData::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("xs"), xs);
    json.insert(QStringLiteral("nonce"), nonce);
    if (!part.isEmpty()) {
        json.insert(QStringLiteral("part"), part);
    }
    return json;
}

SlateV4::ParticipantData SlateV4::ParticipantData::fromJson(const QJsonObject &json)
{
    ParticipantData data;
    data.xs = json.value(QStringLiteral("xs")).toString();
    data.nonce = json.value(QStringLiteral("nonce")).toString();
    data.part = json.value(QStringLiteral("part")).toString();
    return data;
}

QJsonObject SlateV4::PaymentProof::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("saddr"), senderAddress);
    json.insert(QStringLiteral("raddr"), receiverAddress);
    if (!receiverSignature.isEmpty()) {
        json.insert(QStringLiteral("rsig"), receiverSignature);
    }
    return json;
}

SlateV4::PaymentProof SlateV4::PaymentProof::fromJson(const QJsonObject &json)
{
    PaymentProof proof;
    proof.senderAddress = json.value(QStringLiteral("saddr")).toString();
    proof.receiverAddress = json.value(QStringLiteral("raddr")).toString();
    proof.receiverSignature = json.value(QStringLiteral("rsig")).toString();
    return proof;
}

QJsonObject SlateV4::Commit::toJson() const
{
    QJsonObject json;
    if (feature != 0) {
        json.insert(QStringLiteral("f"), feature);
    }
    json.insert(QStringLiteral("c"), commitment);
    if (!proof.isEmpty()) {
        json.insert(QStringLiteral("p"), proof);
    }
    return json;
}

SlateV4::Commit SlateV4::Commit::fromJson(const QJsonObject &json)
{
    Commit commit;
    commit.feature = json.value(QStringLiteral("f")).toInt();
    commit.commitment = json.value(QStringLiteral("c")).toString();
    commit.proof = json.value(QStringLiteral("p")).toString();
    return commit;
}

SlateV4::SlateV4() :
    state(Unknown),
    numParticipants(2),
    kernelFeatures(0),
    hasPaymentProof(false)
{
    id = QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString SlateV4::stateCode() const
{
    switch (state) {
    case Standard1: return QStringLiteral("S1");
    case Standard2: return QStringLiteral("S2");
    case Standard3: return QStringLiteral("S3");
    case Invoice1: return QStringLiteral("I1");
    case Invoice2: return QStringLiteral("I2");
    case Invoice3: return QStringLiteral("I3");
    case Unknown:
    default:
        return QStringLiteral("NA");
    }
}

QString SlateV4::versionCode() const
{
    return QStringLiteral("%1:%2").arg(ver.slateVersion).arg(ver.blockHeaderVersion);
}

QString SlateV4::modeCode() const
{
    if (state == Invoice1 || state == Invoice2 || state == Invoice3) {
        return QStringLiteral("invoice");
    }
    if (state == Standard1 || state == Standard2 || state == Standard3) {
        return QStringLiteral("send");
    }
    return QStringLiteral("unknown");
}

bool SlateV4::isFinalState() const
{
    return state == Standard3 || state == Invoice3;
}

QString SlateV4::workflowId() const
{
    return metadata.value(QStringLiteral("workflow_id")).toString();
}

QString SlateV4::note() const
{
    return metadata.value(QStringLiteral("note")).toString();
}

QString SlateV4::network() const
{
    return metadata.value(QStringLiteral("network")).toString();
}

void SlateV4::setStateFromCode(const QString &code)
{
    state = stateFromCode(code);
}

void SlateV4::advanceState()
{
    switch (state) {
    case Standard1: state = Standard2; break;
    case Standard2: state = Standard3; break;
    case Invoice1: state = Invoice2; break;
    case Invoice2: state = Invoice3; break;
    default: break;
    }
}

QJsonObject SlateV4::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("ver"), versionCode());
    json.insert(QStringLiteral("id"), id);
    json.insert(QStringLiteral("sta"), stateCode());
    if (!offset.isEmpty()) {
        json.insert(QStringLiteral("off"), offset);
    }
    if (numParticipants != 2) {
        json.insert(QStringLiteral("num_parts"), numParticipants);
    }
    if (!amount.isEmpty()) {
        json.insert(QStringLiteral("amt"), amount);
    }
    if (!fee.isEmpty()) {
        json.insert(QStringLiteral("fee"), fee);
    }
    if (kernelFeatures != 0) {
        json.insert(QStringLiteral("feat"), kernelFeatures);
    }
    if (!ttl.isEmpty()) {
        json.insert(QStringLiteral("ttl"), ttl);
    }

    QJsonArray sigs;
    for (int i = 0; i < signatures.size(); ++i) {
        sigs.append(signatures.at(i).toJson());
    }
    json.insert(QStringLiteral("sigs"), sigs);

    if (!commitments.isEmpty()) {
        QJsonArray coms;
        for (int i = 0; i < commitments.size(); ++i) {
            coms.append(commitments.at(i).toJson());
        }
        json.insert(QStringLiteral("coms"), coms);
    }

    if (hasPaymentProof) {
        json.insert(QStringLiteral("proof"), paymentProof.toJson());
    }

    const QStringList keys = metadata.keys();
    for (int i = 0; i < keys.size(); ++i) {
        json.insert(keys.at(i), metadata.value(keys.at(i)));
    }
    return json;
}

SlateV4 SlateV4::fromJson(const QJsonObject &json)
{
    SlateV4 slate;
    slate.id = json.value(QStringLiteral("id")).toString();

    const QString verCode = json.value(QStringLiteral("ver")).toString(QStringLiteral("4:3"));
    const QStringList verParts = verCode.split(QLatin1Char(':'));
    if (verParts.size() == 2) {
        slate.ver.slateVersion = verParts.at(0).toInt();
        slate.ver.blockHeaderVersion = verParts.at(1).toInt();
    }

    slate.setStateFromCode(json.value(QStringLiteral("sta")).toString(QStringLiteral("NA")));
    slate.offset = json.value(QStringLiteral("off")).toString();
    slate.numParticipants = json.value(QStringLiteral("num_parts")).toInt(2);
    slate.amount = json.value(QStringLiteral("amt")).toVariant().toString();
    slate.fee = json.value(QStringLiteral("fee")).toVariant().toString();
    slate.kernelFeatures = json.value(QStringLiteral("feat")).toInt();
    slate.ttl = json.value(QStringLiteral("ttl")).toVariant().toString();

    const QJsonArray sigs = json.value(QStringLiteral("sigs")).toArray();
    for (int i = 0; i < sigs.size(); ++i) {
        if (sigs.at(i).isObject()) {
            slate.signatures.append(ParticipantData::fromJson(sigs.at(i).toObject()));
        }
    }

    const QJsonArray coms = json.value(QStringLiteral("coms")).toArray();
    for (int i = 0; i < coms.size(); ++i) {
        if (coms.at(i).isObject()) {
            slate.commitments.append(Commit::fromJson(coms.at(i).toObject()));
        }
    }

    if (json.value(QStringLiteral("proof")).isObject()) {
        slate.hasPaymentProof = true;
        slate.paymentProof = PaymentProof::fromJson(json.value(QStringLiteral("proof")).toObject());
    }

    const QStringList reservedKeys = QStringList()
            << QStringLiteral("ver")
            << QStringLiteral("id")
            << QStringLiteral("sta")
            << QStringLiteral("off")
            << QStringLiteral("num_parts")
            << QStringLiteral("amt")
            << QStringLiteral("fee")
            << QStringLiteral("feat")
            << QStringLiteral("ttl")
            << QStringLiteral("sigs")
            << QStringLiteral("coms")
            << QStringLiteral("proof")
            << QStringLiteral("feat_args");

    const QStringList keys = json.keys();
    for (int i = 0; i < keys.size(); ++i) {
        if (!reservedKeys.contains(keys.at(i))) {
            slate.metadata.insert(keys.at(i), json.value(keys.at(i)));
        }
    }

    return slate;
}

SlateV4 SlateV4::fromJsonString(const QString &json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    return document.isObject() ? fromJson(document.object()) : SlateV4();
}

SlateV4::State SlateV4::stateFromCode(const QString &code)
{
    if (code == QStringLiteral("S1")) return Standard1;
    if (code == QStringLiteral("S2")) return Standard2;
    if (code == QStringLiteral("S3")) return Standard3;
    if (code == QStringLiteral("I1")) return Invoice1;
    if (code == QStringLiteral("I2")) return Invoice2;
    if (code == QStringLiteral("I3")) return Invoice3;
    return Unknown;
}
