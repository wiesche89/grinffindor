#ifndef WALLETCRYPTOHELPERS_H
#define WALLETCRYPTOHELPERS_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVector>

#include "slatev4.h"

class Output;
class TxKernel;

extern "C" {
#include "secp256k1.h"
#include "secp256k1_aggsig.h"
#include "secp256k1_bulletproofs.h"
#include "secp256k1_commitment.h"
}

namespace WalletCryptoHelpers
{
secp256k1_context *walletSecpContext();
secp256k1_bulletproof_generators *walletBulletproofGenerators();

QByteArray hashBytes(const QByteArray &input);
QString toHex(const unsigned char *data, int size);
QString bech32Encode(const QString &hrp, const QByteArray &payload);

void appendU8(QByteArray &out, quint8 value);
void appendU16(QByteArray &out, quint16 value);
void appendU64(QByteArray &out, quint64 value);

QByteArray fromHex(const QString &hex);
QByteArray deriveValidSecretBytes(const QString &domain, const QString &left, const QString &right);
QByteArray deriveSigningBaseSecret(const QString &walletFingerprint,
                                   const QString &workflowId,
                                   const QString &roleTag);
QByteArray deriveAggsigSecnonce(const QString &walletFingerprint,
                                const QString &workflowId,
                                const QString &roleTag);
QByteArray paymentProofMessage(const SlateV4 &slate);

QString createCompressedPubkeyHex(const QByteArray &secretKey);
bool parsePubkey(const QString &hex, secp256k1_pubkey *pubkey);
QString serializePubkey(const secp256k1_pubkey &pubkey);
bool combinePubkeys(const QList<QString> &hexPubkeys, secp256k1_pubkey *combined);

quint64 amountToNanogrin(const QString &amount);
bool addScalars(const QByteArray &left, const QByteArray &right, QByteArray *sumOut);
bool subtractScalars(const QByteArray &left, const QByteArray &right, QByteArray *differenceOut);

bool parseCommitmentHex(const QString &hex, secp256k1_pedersen_commitment *commitmentOut);
bool buildZeroValueCommitment(const QString &blindHex, secp256k1_pedersen_commitment *commitmentOut);
bool buildValueOnlyCommitment(quint64 value, secp256k1_pedersen_commitment *commitmentOut);
QString serializeCommitment(const secp256k1_pedersen_commitment &commitment);

bool appendFixedHexBytes(QByteArray *serialized, const QString &hex, int expectedSize);
bool appendOutputFeatureForOrdering(QByteArray *serialized, const QString &feature);
bool appendKernelFeaturesForOrdering(QByteArray *serialized, const TxKernel &kernel);
bool verifyOutputsBatchRangeproofs(const QVector<Output> &outputs, QString *errorOut);
bool verifyOutputRangeproof(const Output &output, secp256k1_scratch_space *scratch, QString *errorOut);

QByteArray buildKernelSignatureMessageForFeature(const QString &feature,
                                                 quint64 fee,
                                                 quint64 lockHeight,
                                                 QString *errorOut);
QByteArray buildKernelSignatureMessage(const SlateV4 &slate);

bool createCommitmentAndRangeproof(const QString &walletFingerprint,
                                   const QString &workflowId,
                                   const QString &roleTag,
                                   const QString &amount,
                                   SlateV4::Commit *commitOut);
bool createCommitmentAndRangeproofFromSecrets(const QByteArray &blind,
                                              const QByteArray &privateNonceHash,
                                              const QByteArray &rewindNonceHash,
                                              const QByteArray &proofMessage,
                                              quint64 value,
                                              SlateV4::Commit *commitOut);

int findParticipantIndex(const SlateV4 &slate, const QString &publicBlind);
bool createPartialSignature(const QByteArray &messageHash,
                            const QByteArray &seckey,
                            const QByteArray &secnonce,
                            const secp256k1_pubkey &pubnonceTotal,
                            const secp256k1_pubkey &pubkeyTotal,
                            QByteArray *signatureOut);
bool aggsigRawToCompact(const QByteArray &rawSignature, QByteArray *compactOut);
bool aggsigCompactToRaw(const QByteArray &compactSignature, QByteArray *rawOut);
bool kernelSigRawToCompact(const QByteArray &rawSignature, QByteArray *compactOut);
bool kernelSigCompactToRaw(const QByteArray &compactSignature, QByteArray *rawOut);
bool verifyPartialSignature(const QByteArray &signature,
                            const QByteArray &messageHash,
                            const secp256k1_pubkey &pubnonceTotal,
                            const secp256k1_pubkey &pubkey,
                            const secp256k1_pubkey &pubkeyTotal);
}

#endif // WALLETCRYPTOHELPERS_H
