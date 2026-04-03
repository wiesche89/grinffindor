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
quint64 amountToNanogrin(const QString &amount);
QString formatNanogrin(quint64 amount);
WalletOutput findTrackedOutputByCommitment(const QList<WalletOutput> &outputs, const QString &commitment);
WalletOutput normalizedTrackedOutput(const WalletOutput &output, const WalletKeychain &keychain);
QString invoiceContextKey(const QString &suffix);
QString standardContextKey(const QString &suffix);
WalletCryptoBackend::ParticipantContext participantContextFromJson(const QJsonObject &json,
                                                                  const QString &role);
QJsonObject participantContextToJson(const WalletCryptoBackend::ParticipantContext &context);
QList<SlateV4::Commit> sortedCompactCommitments(const QList<SlateV4::Commit> &commits);
QString encodeSlatepackArmor(const QString &payloadJson, const QString &sender);
QString decodeIncomingSlatepack(const QString &input, const QByteArray &decryptionKey);
}

#endif
