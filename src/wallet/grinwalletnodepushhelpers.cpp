#include "grinwalletnodepushhelpers.h"

#include <QJsonArray>
#include <QUrlQuery>

#include "input.h"
#include "output.h"
#include "outputfeatures.h"
#include "transactionbody.h"
#include "txkernel.h"

namespace {

/**
 * @brief appendExactHexBytes
 * @param out
 * @param hex
 * @param expectedSize
 * @return
 */
bool appendExactHexBytes(QByteArray *out, const QString &hex, int expectedSize)
{
    if (!out) {
        return false;
    }

    const QByteArray bytes = QByteArray::fromHex(hex.trimmed().toUtf8());
    if (bytes.size() != expectedSize) {
        return false;
    }

    out->append(bytes);
    return true;
}

/**
 * @brief appendU64Network
 * @param out
 * @param value
 */
void appendU64Network(QByteArray *out, quint64 value)
{
    if (!out) {
        return;
    }

    for (int shift = 56; shift >= 0; shift -= 8) {
        out->append(static_cast<char>((value >> shift) & 0xff));
    }
}

/**
 * @brief inputFeatureName
 * @param feature
 * @return
 */
QString inputFeatureName(OutputFeatures::Feature feature)
{
    return feature == OutputFeatures::Coinbase
        ? QStringLiteral("Coinbase")
        : QStringLiteral("Plain");
}

/**
 * @brief serializeKernelFeatures
 * @param kernel
 * @return
 */
QJsonObject serializeKernelFeatures(const TxKernel &kernel)
{
    QJsonObject featureArgs;
    if (kernel.features().isEmpty() || kernel.features() == QStringLiteral("Plain")) {
        featureArgs.insert(QStringLiteral("fee"), static_cast<qint64>(kernel.fee()));
    }

    QJsonObject features;
    features.insert(kernel.features().isEmpty() ? QStringLiteral("Plain") : kernel.features(), featureArgs);
    return features;
}

} // namespace

/**
 * @brief GrinWalletNodePushHelpers::serializeTransactionForPoolV1
 * @param tx
 * @param errorOut
 * @return
 */
QByteArray GrinWalletNodePushHelpers::serializeTransactionForPoolV1(const Transaction &tx, QString *errorOut)
{
    QByteArray encoded;

    if (!appendExactHexBytes(&encoded, tx.offset().hex(), 32)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Invalid transaction offset for v1 binary serialization.");
        }
        return QByteArray();
    }

    const TransactionBody body = tx.body();
    const QVector<Input> inputs = body.inputs();
    const QVector<Output> outputs = body.outputs();
    const QVector<TxKernel> kernels = body.kernels();

    appendU64Network(&encoded, static_cast<quint64>(inputs.size()));
    appendU64Network(&encoded, static_cast<quint64>(outputs.size()));

    appendU64Network(&encoded, static_cast<quint64>(kernels.size()));

    for (const Input &input : inputs) {
        const quint8 feature = input.features() == OutputFeatures::Coinbase ? 1 : 0;
        encoded.append(static_cast<char>(feature));
        if (!appendExactHexBytes(&encoded, input.commit().hex(), 33)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Invalid input commitment for v1 binary serialization.");
            }
            return QByteArray();
        }
    }

    for (const Output &output : outputs) {
        const QString featureName = output.features().trimmed();
        const quint8 feature = featureName == QStringLiteral("Coinbase") ? 1 : 0;
        encoded.append(static_cast<char>(feature));
        if (!appendExactHexBytes(&encoded, output.commit(), 33)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Invalid output commitment for v1 binary serialization.");
            }
            return QByteArray();
        }

        const QByteArray proof = QByteArray::fromHex(output.proof().trimmed().toUtf8());
        if (proof.isEmpty()) {
            if (errorOut) {
                *errorOut = QStringLiteral("Missing output proof for v1 binary serialization.");
            }
            return QByteArray();
        }

        appendU64Network(&encoded, static_cast<quint64>(proof.size()));
        encoded.append(proof);
    }

    for (const TxKernel &kernel : kernels) {
        const QString featureName = kernel.features().trimmed();
        quint8 featureByte = 0;
        if (featureName.isEmpty() || featureName == QStringLiteral("Plain")) {
            featureByte = 0;
        } else if (featureName == QStringLiteral("Coinbase")) {
            featureByte = 1;
        } else {
            if (errorOut) {
                *errorOut = QStringLiteral("Unsupported kernel feature for v1 binary serialization: %1")
                                .arg(featureName);
            }
            return QByteArray();
        }

        encoded.append(static_cast<char>(featureByte));
        appendU64Network(&encoded, featureByte == 1 ? 0 : static_cast<quint64>(kernel.fee()));
        appendU64Network(&encoded, 0);

        if (!appendExactHexBytes(&encoded, kernel.excess(), 33)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Invalid kernel excess for v1 binary serialization.");
            }
            return QByteArray();
        }
        if (!appendExactHexBytes(&encoded, kernel.excessSig(), 64)) {
            if (errorOut) {
                *errorOut = QStringLiteral("Invalid kernel signature for v1 binary serialization.");
            }
            return QByteArray();
        }
    }

    return encoded;
}

/**
 * @brief GrinWalletNodePushHelpers::poolPushCandidateUrlsForApiUrl
 * @param apiUrl
 * @param fluff
 * @return
 */
QList<QUrl> GrinWalletNodePushHelpers::poolPushCandidateUrlsForApiUrl(const QString &apiUrl, bool fluff)
{
    QUrl url(apiUrl);

    QString path = url.path();
    if (path.endsWith(QStringLiteral("/v2/foreign"))) {
        path.chop(QStringLiteral("/v2/foreign").size());
    } else if (path.endsWith(QStringLiteral("/v2/foreign/"))) {
        path.chop(QStringLiteral("/v2/foreign/").size());
    }

    if (!path.endsWith('/')) {
        path.append('/');
    }

    const QString basePath = path;
    const QStringList relativeCandidates{
        QStringLiteral("v1/pool/push"),
        QStringLiteral("v1/pool/push/"),
        QStringLiteral("v1/pool/push_tx"),
        QStringLiteral("v1/pool/push_tx/")
    };

    QList<QUrl> urls;

    urls.reserve(relativeCandidates.size());
    for (const QString &candidate : relativeCandidates) {
        QUrl candidateUrl = url;
        candidateUrl.setPath(basePath + candidate);
        QUrlQuery query;
        if (fluff) {
            query.addQueryItem(QStringLiteral("fluff"), QString());
        }
        candidateUrl.setQuery(query);
        urls.append(candidateUrl);
    }
    return urls;
}

/**
 * @brief GrinWalletNodePushHelpers::serializeTransactionForNode
 * @param tx
 * @return
 */
QJsonObject GrinWalletNodePushHelpers::serializeTransactionForNode(const Transaction &tx)
{
    QJsonObject txJson;
    txJson.insert(QStringLiteral("offset"), tx.offset().hex());

    const TransactionBody body = tx.body();

    QJsonArray inputsJson;

    const QVector<Input> inputs = body.inputs();
    for (const Input &input : inputs) {
        QJsonObject inputJson;
        inputJson.insert(QStringLiteral("features"), inputFeatureName(input.features()));
        inputJson.insert(QStringLiteral("commit"), input.commit().hex());
        inputsJson.append(inputJson);
    }

    QJsonArray outputsJson;

    const QVector<Output> outputs = body.outputs();
    for (const Output &output : outputs) {
        QJsonObject outputJson;
        outputJson.insert(QStringLiteral("features"), output.features());
        outputJson.insert(QStringLiteral("commit"), output.commit());
        outputJson.insert(QStringLiteral("proof"), output.proof());
        outputsJson.append(outputJson);
    }

    QJsonArray kernelsJson;

    const QVector<TxKernel> kernels = body.kernels();
    for (const TxKernel &kernel : kernels) {
        QJsonObject kernelJson;
        kernelJson.insert(QStringLiteral("features"), serializeKernelFeatures(kernel));
        kernelJson.insert(QStringLiteral("excess"), kernel.excess());
        kernelJson.insert(QStringLiteral("excess_sig"), kernel.excessSig());
        kernelsJson.append(kernelJson);
    }

    QJsonObject bodyJson;
    bodyJson.insert(QStringLiteral("inputs"), inputsJson);
    bodyJson.insert(QStringLiteral("outputs"), outputsJson);
    bodyJson.insert(QStringLiteral("kernels"), kernelsJson);
    txJson.insert(QStringLiteral("body"), bodyJson);
    return txJson;
}

/**
 * @brief GrinWalletNodePushHelpers::serializeTransactionForNodeLegacyKernel
 * @param tx
 * @return
 */
QJsonObject GrinWalletNodePushHelpers::serializeTransactionForNodeLegacyKernel(const Transaction &tx)
{
    QJsonObject txJson;
    txJson.insert(QStringLiteral("offset"), tx.offset().hex());

    const TransactionBody body = tx.body();

    QJsonArray inputsJson;

    const QVector<Input> inputs = body.inputs();
    for (const Input &input : inputs) {
        QJsonObject inputJson;
        inputJson.insert(QStringLiteral("features"), inputFeatureName(input.features()));
        inputJson.insert(QStringLiteral("commit"), input.commit().hex());
        inputsJson.append(inputJson);
    }

    QJsonArray outputsJson;

    const QVector<Output> outputs = body.outputs();
    for (const Output &output : outputs) {
        QJsonObject outputJson;
        outputJson.insert(QStringLiteral("features"), output.features());
        outputJson.insert(QStringLiteral("commit"), output.commit());
        outputJson.insert(QStringLiteral("proof"), output.proof());
        outputsJson.append(outputJson);
    }

    QJsonArray kernelsJson;

    const QVector<TxKernel> kernels = body.kernels();
    for (const TxKernel &kernel : kernels) {
        QJsonObject kernelJson;
        const QString kernelFeature =
            kernel.features().isEmpty() ? QStringLiteral("Plain") : kernel.features();
        kernelJson.insert(QStringLiteral("features"), kernelFeature);
        kernelJson.insert(QStringLiteral("fee"), static_cast<qint64>(kernel.fee()));
        kernelJson.insert(QStringLiteral("excess"), kernel.excess());
        kernelJson.insert(QStringLiteral("excess_sig"), kernel.excessSig());
        kernelsJson.append(kernelJson);
    }

    QJsonObject bodyJson;
    bodyJson.insert(QStringLiteral("inputs"), inputsJson);
    bodyJson.insert(QStringLiteral("outputs"), outputsJson);
    bodyJson.insert(QStringLiteral("kernels"), kernelsJson);
    txJson.insert(QStringLiteral("body"), bodyJson);
    return txJson;
}
