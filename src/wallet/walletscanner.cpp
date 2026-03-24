#include "walletscanner.h"

#include <QStringList>

#include "outputprintable.h"
#include "walletkeychain.h"

namespace {

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

QString formatNanogrin(quint64 amount)
{
    return QStringLiteral("%1.%2")
        .arg(QString::number(amount / 1000000000ULL))
        .arg(QString::number(amount % 1000000000ULL), 9, QLatin1Char('0'));
}

}

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

QJsonArray WalletScanner::outputsToJson(const QList<WalletOutput> &outputs)
{
    QJsonArray array;
    for (int i = 0; i < outputs.size(); ++i) {
        array.append(outputs.at(i).toJson());
    }
    return array;
}

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

QList<WalletOutput> WalletScanner::reconcileTrackedOutputs(const QList<WalletOutput> &trackedOutputs,
                                                           const QList<OutputPrintable> &chainOutputs)
{
    QList<WalletOutput> reconciled = trackedOutputs;
    for (int i = 0; i < reconciled.size(); ++i) {
        reconciled[i].onChain = false;
        reconciled[i].spent = true;
        reconciled[i].height = 0;
    }

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
        output.blindingFactor = rewound.blindingFactor;
        output.childIndex = rewound.childIndex;
        output.height = chainOutput.blockHeight().toULongLong();
        output.coinbase =
            chainOutput.outputType() == OutputPrintable::OutputType::OutputTypeCoinbase;
        output.onChain = true;
        output.spent = chainOutput.spent();
        output.locked = false;
        discovered.append(output);
    }

    return discovered;
}

QJsonObject WalletScanner::balancesFromOutputs(const QList<WalletOutput> &outputs, qulonglong chainHeight)
{
    quint64 total = 0;
    quint64 spendable = 0;
    quint64 locked = 0;
    quint64 immature = 0;

    for (int i = 0; i < outputs.size(); ++i) {
        const WalletOutput &output = outputs.at(i);
        if (output.spent) {
            continue;
        }

        const quint64 amount = amountToNanogrin(output.amount);
        total += amount;

        if (output.locked) {
            locked += amount;
            continue;
        }

        const bool coinbaseMature = !output.coinbase || (output.height > 0 && chainHeight >= output.height + 1000);
        const bool confirmed = output.onChain && output.height > 0 && chainHeight >= output.height + 10;
        if (coinbaseMature && confirmed) {
            spendable += amount;
        } else {
            immature += amount;
        }
    }

    QJsonObject balances;
    balances.insert(QStringLiteral("total"), formatNanogrin(total));
    balances.insert(QStringLiteral("spendable"), formatNanogrin(spendable));
    balances.insert(QStringLiteral("locked"), formatNanogrin(locked));
    balances.insert(QStringLiteral("immature"), formatNanogrin(immature));
    return balances;
}
