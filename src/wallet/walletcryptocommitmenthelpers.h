#ifndef WALLETCRYPTOCOMMITMENTHELPERS_H
#define WALLETCRYPTOCOMMITMENTHELPERS_H

#include <QString>
#include <QVector>

#include "slatev4.h"

class Output;
class TxKernel;

extern "C" {
#include "secp256k1_bulletproofs.h"
#include "secp256k1_commitment.h"
}

namespace WalletCryptoHelpers
{
bool parseCommitmentHex(const QString &hex, secp256k1_pedersen_commitment *commitmentOut);
bool buildZeroValueCommitment(const QString &blindHex, secp256k1_pedersen_commitment *commitmentOut);
bool buildValueOnlyCommitment(quint64 value, secp256k1_pedersen_commitment *commitmentOut);
QString serializeCommitment(const secp256k1_pedersen_commitment &commitment);

bool appendFixedHexBytes(QByteArray *serialized, const QString &hex, int expectedSize);
bool appendOutputFeatureForOrdering(QByteArray *serialized, const QString &feature);
bool appendKernelFeaturesForOrdering(QByteArray *serialized, const TxKernel &kernel);
bool verifyOutputsBatchRangeproofs(const QVector<Output> &outputs, QString *errorOut);
bool verifyOutputRangeproof(const Output &output, secp256k1_scratch_space *scratch, QString *errorOut);

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
}

#endif // WALLETCRYPTOCOMMITMENTHELPERS_H
