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

/**
 * @brief Returns to json.
 */
        QJsonObject toJson() const;
/**
 * @brief Returns from json.
 */
        static ParticipantData fromJson(const QJsonObject &json);
    };

    struct PaymentProof {
        QString senderAddress;
        QString receiverAddress;
        QString receiverSignature;

/**
 * @brief Returns to json.
 */
        QJsonObject toJson() const;
/**
 * @brief Returns from json.
 */
        static PaymentProof fromJson(const QJsonObject &json);
    };

    struct Commit {
        int feature = 0;
        QString commitment;
        QString proof;

/**
 * @brief Returns to json.
 */
        QJsonObject toJson() const;
/**
 * @brief Returns from json.
 */
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

/**
 * @brief Processes slate v4.
 */
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

/**
 * @brief Returns state code.
 */
    QString stateCode() const;
/**
 * @brief Returns version code.
 */
    QString versionCode() const;
/**
 * @brief Returns mode code.
 */
    QString modeCode() const;
/**
 * @brief Returns whether final state.
 */
    bool isFinalState() const;
/**
 * @brief Returns workflow id.
 */
    QString workflowId() const;
/**
 * @brief Returns note.
 */
    QString note() const;
/**
 * @brief Returns network.
 */
    QString network() const;

/**
 * @brief Sets state from code.
 */
    void setStateFromCode(const QString &code);
/**
 * @brief Advances state.
 */
    void advanceState();

/**
 * @brief Returns to json.
 */
    QJsonObject toJson() const;
/**
 * @brief Returns from json.
 */
    static SlateV4 fromJson(const QJsonObject &json);
/**
 * @brief Returns from json string.
 */
    static SlateV4 fromJsonString(const QString &json);

private:
/**
 * @brief Returns state from code.
 */
    static State stateFromCode(const QString &code);
};

#endif // SLATEV4_H
