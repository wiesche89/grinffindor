#ifndef SLATEV4_H
#define SLATEV4_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

class SlateV4
{
public:
    struct Version {
        int slateVersion = 4;
        int blockHeaderVersion = 3;
    };

    struct ParticipantData {
        QString xs;
        QString nonce;
        QString part;

        QJsonObject toJson() const;
        static ParticipantData fromJson(const QJsonObject &json);
    };

    struct PaymentProof {
        QString senderAddress;
        QString receiverAddress;
        QString receiverSignature;

        QJsonObject toJson() const;
        static PaymentProof fromJson(const QJsonObject &json);
    };

    struct Commit {
        int feature = 0;
        QString commitment;
        QString proof;

        QJsonObject toJson() const;
        static Commit fromJson(const QJsonObject &json);
    };

    enum State {
        Unknown,
        Standard1,
        Standard2,
        Standard3,
        Invoice1,
        Invoice2,
        Invoice3
    };

    SlateV4();

    QString id;
    Version ver;
    State state;
    QString offset;
    int numParticipants;
    QString amount;
    QString fee;
    int kernelFeatures;
    QString ttl;
    QList<ParticipantData> signatures;
    QList<Commit> commitments;
    bool hasPaymentProof;
    PaymentProof paymentProof;
    QJsonObject metadata;

    QString stateCode() const;
    QString versionCode() const;
    QString modeCode() const;
    bool isFinalState() const;
    QString workflowId() const;
    QString note() const;
    QString network() const;

    void setStateFromCode(const QString &code);
    void advanceState();

    QJsonObject toJson() const;
    static SlateV4 fromJson(const QJsonObject &json);
    static SlateV4 fromJsonString(const QString &json);

private:
    static State stateFromCode(const QString &code);
};

#endif // SLATEV4_H
