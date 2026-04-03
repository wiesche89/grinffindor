#include "walletscanner.h"

#include <QStringList>

#include "outputprintable.h"
#include "walletkeychain.h"

namespace {

/**
 * @brief amountToNanogrin
 * @param amount
 * @return
 */
quint64 amountToNanogrin(const QString &amount)
{
    const QString trimmed = amount.trimmed();
    if (trimmed.isEmpty()) {
        return 0;
    }

    const QStringList parts = trimmed.split(QLatin1Char('.'));
    if (parts.isEmpty() || parts.size() > 2) {
        return 0;
    }

    bool wholeOk = false;
    const quint64 whole = parts.at(0).toULongLong(&wholeOk);
    if (!wholeOk) {
        return 0;
    }

    QString fractional = parts.size() == 2 ? parts.at(1) : QString();
    if (fractional.size() > 9) {
        fractional = fractional.left(9);
    }
    while (fractional.size() < 9) {
        fractional.append(QLatin1Char('0'));
    }

    bool fracOk = false;
    const quint64 frac = fractional.isEmpty() ? 0 : fractional.toULongLong(&fracOk);
    if (!fractional.isEmpty() && !fracOk) {
        return 0;
    }

    return whole * 1000000000ULL + frac;
}

/**
 * @brief formatNanogrin
 * @param amount
 * @return
 */
QString formatNanogrin(quint64 amount)
{
    return QStringLiteral("%1.%2")
        .arg(QString::number(amount / 1000000000ULL))
        .arg(QString::number(amount % 1000000000ULL), 9, QLatin1Char('0'));
}

}

/**
 * @brief WalletScanner::outputsFromState
 * @param walletState
 * @return
 */
QList<WalletOutput> WalletScanner::outputsFromState(const QJsonObject &walletState)
{
    QList<WalletOutput> outputs;
    const QJsonArray array = walletState.value(QStringLiteral("outputs")).toArray();

    outputs.reserve(array.size());
    for (int i = 0; i < array.size(); ++i) {
        if (array.at(i).isObject()) {
            outputs.append(WalletOutput::fromJson(array.at(i).toObject()));
        }
    }
    return outputs;
}

/**
 * @brief WalletScanner::outputsToJson
 * @param outputs
 * @return
 */
QJsonArray WalletScanner::outputsToJson(const QList<WalletOutput> &outputs)
{
    QJsonArray array;
    for (int i = 0; i < outputs.size(); ++i) {
        array.append(outputs.at(i).toJson());
    }
    return array;
}

/**
 * @brief WalletScanner::commitmentsToJson
 * @param outputs
 * @return
 */
QJsonArray WalletScanner::commitmentsToJson(const QList<WalletOutput> &outputs)
{
    QJsonArray commits;
    for (int i = 0; i < outputs.size(); ++i) {
        if (!outputs.at(i).commitment.isEmpty()) {
            commits.append(outputs.at(i).commitment);
        }
    }
    return commits;
}

/**
 * @brief WalletScanner::reconcileTrackedOutputs
 * @param trackedOutputs
 * @param chainOutputs
 * @return
 */
QList<WalletOutput> WalletScanner::reconcileTrackedOutputs(const QList<WalletOutput> &trackedOutputs,
                                                           const QList<OutputPrintable> &chainOutputs)
{
    QList<WalletOutput> reconciled = trackedOutputs;

    // Only update outputs that ARE found on chain
    // Outputs not found keep their previous state (may be pending, local, or unconfirmed)
    for (int i = 0; i < chainOutputs.size(); ++i) {
        const OutputPrintable &chainOutput = chainOutputs.at(i);
        const QString commitHex = chainOutput.commit().hex();
        for (int j = 0; j < reconciled.size(); ++j) {
            if (reconciled[j].commitment == commitHex) {
                reconciled[j].onChain = true;
                reconciled[j].spent = chainOutput.spent();
                reconciled[j].height = chainOutput.blockHeight().toULongLong();
                reconciled[j].pending = false;
                if (!chainOutput.spent()) {
                    reconciled[j].locked = false;
                }
                reconciled[j].coinbase =
                    chainOutput.outputType() == OutputPrintable::OutputType::OutputTypeCoinbase;
            }
        }
    }

    return reconciled;
}

/**
 * @brief WalletScanner::discoverOwnedOutputs
 * @param chainOutputs
 * @param keychain
 * @return
 */
QList<WalletOutput> WalletScanner::discoverOwnedOutputs(const QList<OutputPrintable> &chainOutputs,
                                                        const WalletKeychain &keychain)
{
    QList<WalletOutput> discovered;
    if (!keychain.isValid()) {
        return discovered;
    }

    for (int i = 0; i < chainOutputs.size(); ++i) {
        const OutputPrintable &chainOutput = chainOutputs.at(i);
        const WalletKeychain::RewindResult rewound = keychain.rewindOutputProof(
            chainOutput.commit().hex(),
            chainOutput.proof());
        if (!rewound.success) {
            continue;
        }

        WalletOutput output;
        output.commitment = chainOutput.commit().hex();
        output.proof = chainOutput.proof();
        output.amount = formatNanogrin(rewound.amount);
        output.source = QStringLiteral("scan");
        output.keyPath = rewound.keyPath;
        output.childIndex = rewound.childIndex;
        output.height = chainOutput.blockHeight().toULongLong();
        output.coinbase =
            chainOutput.outputType() == OutputPrintable::OutputType::OutputTypeCoinbase;
        output.onChain = true;
        output.spent = chainOutput.spent();
        output.locked = false;
        const WalletKeychain::OutputSecrets secrets =
            keychain.deriveOutputSecrets(rewound.childIndex, rewound.amount);
        if (secrets.success) {
            output.blindingFactor = QString::fromUtf8(secrets.blindingFactor.toHex());
        } else {
            output.blindingFactor = rewound.blindingFactor;
        }
        discovered.append(output);
    }

    return discovered;
}

/**
 * @brief WalletScanner::balancesFromOutputs
 * @param outputs
 * @param chainHeight
 * @return
 */
QJsonObject WalletScanner::balancesFromOutputs(const QList<WalletOutput> &outputs, qulonglong chainHeight)
{
    // Balance categories following grin-wallet reference implementation
    // See libwallet/src/internal/updater.rs::retrieve_info()
    
    quint64 total = 0;
    quint64 spendable = 0;
    quint64 locked = 0;
    quint64 immature = 0;
    quint64 awaiting_confirmation = 0;
    quint64 awaiting_finalization = 0;

    for (int i = 0; i < outputs.size(); ++i) {
        const WalletOutput &output = outputs.at(i);
        const quint64 amount = amountToNanogrin(output.amount);
        
        // Skip spent outputs - they don't count toward any balance
        if (output.spent) {
            continue;
        }

        total += amount;

        // Priority 1: Check if output is locked (in-flight transaction)
        if (output.locked) {
            locked += amount;
            continue;
        }

        // Priority 2: Check if output is pending (not yet on-chain)
        if (output.pending) {
            // Pending outputs are awaiting confirmation
            if (!output.onChain) {
                awaiting_confirmation += amount;
            } else {
                // Rare case: on-chain but pending flag still set
                awaiting_finalization += amount;
            }
            continue;
        }

        // Priority 3: If not on-chain yet, definitely immature (local output)
        if (!output.onChain) {
            immature += amount;
            continue;
        }

        // For on-chain outputs, check maturity and confirmation
        // Priority 4: Check coinbase maturity (1000 block delay)
        const bool isCoinbase = output.coinbase;
        bool coinbaseMature = true;
        
        if (isCoinbase && output.height > 0) {
            // Coinbase needs 1000 block maturity
            // But if chainHeight is 0 (node not synced), assume it's mature since it's on-chain
            if (chainHeight > 0) {
                coinbaseMature = chainHeight >= output.height + 1000;
            } else {
                // When chainHeight is unknown but output is on-chain, assume mature
                coinbaseMature = true;
            }
        }
        
        // Priority 5: Check if confirmed on-chain (10 block confirmations)
        // Reference formula: num_confirmations = 1 + (chainHeight - height) >= 10
        //   => chainHeight >= height + 9
        bool confirmed = false;
        if (output.height > 0 && chainHeight > 0) {
            confirmed = chainHeight >= output.height + 9;
        } else if (output.height > 0 && chainHeight == 0) {
            // If chainHeight is 0 but output is on-chain, assume it's confirmed
            // since it was persisted
            confirmed = true;
        } else if (!isCoinbase && output.height == 0) {
            // Fallback for node payloads where height is not provided for tx outputs.
            confirmed = true;
        }

        // grin-wallet style split:
        // - coinbase that is not mature -> immature
        // - regular tx output that is on-chain but <10 conf -> awaiting_confirmation
        // - mature + confirmed -> spendable
        if (!coinbaseMature) {
            immature += amount;
        } else if (!confirmed) {
            awaiting_confirmation += amount;
        } else {
            spendable += amount;
        }
    }

    QJsonObject balances;
    balances.insert(QStringLiteral("total"), formatNanogrin(total));
    balances.insert(QStringLiteral("spendable"), formatNanogrin(spendable));
    balances.insert(QStringLiteral("locked"), formatNanogrin(locked));
    balances.insert(QStringLiteral("immature"), formatNanogrin(immature));
    // Extended categories for detailed reporting
    balances.insert(QStringLiteral("awaiting_confirmation"), formatNanogrin(awaiting_confirmation));
    balances.insert(QStringLiteral("awaiting_finalization"), formatNanogrin(awaiting_finalization));

    return balances;
}
