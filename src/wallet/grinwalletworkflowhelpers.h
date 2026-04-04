#ifndef GRINWALLETWORKFLOWHELPERS_H
#define GRINWALLETWORKFLOWHELPERS_H

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

#include "slatev4.h"
#include "walletcryptobackend.h"
#include "walletkeychain.h"
#include "walletoutput.h"

namespace GrinWalletWorkflowHelpers
{
/**
 * @brief Returns amount to nanogrin.
 */
quint64 amountToNanogrin(const QString &amount);
/**
 * @brief Processes format nanogrin.
 */
QString formatNanogrin(quint64 amount);
/**
 * @brief Returns find tracked output by commitment.
 */
WalletOutput findTrackedOutputByCommitment(const QList<WalletOutput> &outputs, const QString &commitment);
/**
 * @brief Returns normalized tracked output.
 */
WalletOutput normalizedTrackedOutput(const WalletOutput &output, const WalletKeychain &keychain);
/**
 * @brief Returns invoice context key.
 */
QString invoiceContextKey(const QString &suffix);
/**
 * @brief Returns standard context key.
 */
QString standardContextKey(const QString &suffix);
WalletCryptoBackend::ParticipantContext participantContextFromJson(const QJsonObject &json,
                                                                  const QString &role);
/**
 * @brief Returns participant context to json.
 */
QJsonObject participantContextToJson(const WalletCryptoBackend::ParticipantContext &context);
/**
 * @brief Returns sorted compact commitments.
 */
QList<SlateV4::Commit> sortedCompactCommitments(const QList<SlateV4::Commit> &commits);
/**
 * @brief Encodes encode slatepack armor.
 */
QString encodeSlatepackArmor(const QString &payloadJson, const QString &sender);
/**
 * @brief Decodes decode incoming slatepack.
 */
QString decodeIncomingSlatepack(const QString &input, const QByteArray &decryptionKey);
}

#endif
