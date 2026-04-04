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
/**
 * @brief Parses parse commitment hex.
 */
bool parseCommitmentHex(const QString &hex, secp256k1_pedersen_commitment *commitmentOut);
/**
 * @brief Builds zero value commitment.
 */
bool buildZeroValueCommitment(const QString &blindHex, secp256k1_pedersen_commitment *commitmentOut);
/**
 * @brief Builds value only commitment.
 */
bool buildValueOnlyCommitment(quint64 value, secp256k1_pedersen_commitment *commitmentOut);
/**
 * @brief Returns serialize commitment.
 */
QString serializeCommitment(const secp256k1_pedersen_commitment &commitment);

/**
 * @brief Returns append fixed hex bytes.
 */
bool appendFixedHexBytes(QByteArray *serialized, const QString &hex, int expectedSize);
/**
 * @brief Returns append output feature for ordering.
 */
bool appendOutputFeatureForOrdering(QByteArray *serialized, const QString &feature);
/**
 * @brief Returns append kernel features for ordering.
 */
bool appendKernelFeaturesForOrdering(QByteArray *serialized, const TxKernel &kernel);
/**
 * @brief Validates verify outputs batch rangeproofs.
 */
bool verifyOutputsBatchRangeproofs(const QVector<Output> &outputs, QString *errorOut);
/**
 * @brief Validates verify output rangeproof.
 */
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
