#include "walletoutput.h"
#include "grinwalletstorage.h"

/**
 * @brief WalletOutput::toJson
 * @return
 */
QJsonObject WalletOutput::toJson() const
{
    QJsonObject json;
    json.insert(QStringLiteral("commitment"), commitment);
    json.insert(QStringLiteral("proof"), proof);
    json.insert(QStringLiteral("amount"), amount);
    json.insert(QStringLiteral("source"), source);
    json.insert(QStringLiteral("height"), QString::number(height));
    json.insert(QStringLiteral("coinbase"), coinbase);
    json.insert(QStringLiteral("on_chain"), onChain);
    json.insert(QStringLiteral("spent"), spent);
    json.insert(QStringLiteral("locked"), locked);
    json.insert(QStringLiteral("pending"), pending);
    json.insert(QStringLiteral("reverted"), reverted);
    json.insert(QStringLiteral("workflow_id"), workflowId);

    // Spend-critical metadata is encrypted at rest when a session key is available.
    const QJsonObject sensitive = {
        { QStringLiteral("key_path"), keyPath },
        { QStringLiteral("blinding_factor"), blindingFactor },
        { QStringLiteral("child_index"), static_cast<int>(childIndex) }
    };
    const QJsonObject envelope = GrinWalletStorage::protectSensitiveObject(sensitive);
    if (!envelope.isEmpty()) {
        json.insert(QStringLiteral("protected_v1"), envelope);
    } else {
        json.insert(QStringLiteral("key_path"), keyPath);
        json.insert(QStringLiteral("blinding_factor"), blindingFactor);
        json.insert(QStringLiteral("child_index"), static_cast<int>(childIndex));
    }
    return json;
}

/**
 * @brief WalletOutput::fromJson
 * @param json
 * @return
 */
WalletOutput WalletOutput::fromJson(const QJsonObject &json)
{
    WalletOutput output;
    output.commitment = json.value(QStringLiteral("commitment")).toString();
    output.proof = json.value(QStringLiteral("proof")).toString();
    output.amount = json.value(QStringLiteral("amount")).toString();
    output.source = json.value(QStringLiteral("source")).toString();
    QJsonObject sensitive;
    if (GrinWalletStorage::unprotectSensitiveObject(json.value(QStringLiteral("protected_v1")).toObject(), &sensitive)) {
        output.keyPath = sensitive.value(QStringLiteral("key_path")).toString();
        output.blindingFactor = sensitive.value(QStringLiteral("blinding_factor")).toString();
        output.childIndex = static_cast<quint32>(sensitive.value(QStringLiteral("child_index")).toInt());
    } else {
        output.keyPath = json.value(QStringLiteral("key_path")).toString();
        output.blindingFactor = json.value(QStringLiteral("blinding_factor")).toString();
        output.childIndex = static_cast<quint32>(json.value(QStringLiteral("child_index")).toInt());
    }
    output.height = json.value(QStringLiteral("height")).toVariant().toULongLong();
    output.coinbase = json.value(QStringLiteral("coinbase")).toBool();
    output.onChain = json.value(QStringLiteral("on_chain")).toBool();
    output.spent = json.value(QStringLiteral("spent")).toBool();
    output.locked = json.value(QStringLiteral("locked")).toBool();
    output.pending = json.value(QStringLiteral("pending")).toBool();
    output.reverted = json.value(QStringLiteral("reverted")).toBool();
    output.workflowId = json.value(QStringLiteral("workflow_id")).toString();

    // Backward-compatibility normalization for older/partial persisted states.
    // If an output has a chain height but was stored with on_chain=false,
    // treat it as on-chain unless it is explicitly pending local workflow state.
    if (!output.onChain
        && output.height > 0
        && !output.pending
        && !output.locked

        && !output.commitment.isEmpty()) {
        output.onChain = true;
    }

    // Spent outputs should not remain in pending/locked/reverted workflow states.
    if (output.spent) {
        output.pending = false;
        output.locked = false;
        output.reverted = false;
    }

    // Reverted outputs are off-chain by definition.
    if (output.reverted) {
        output.onChain = false;
        output.pending = false;
        output.locked = false;
    }

    return output;
}
