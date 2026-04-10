#include <QtTest>
#include <QFile>
#include "slatev4.h"
#include "walletcryptobackend.h"
#include "walletkeychain.h"

#include "../src/submodules/grin-common-api/src/attributes/commitment.h"
#include "../src/submodules/grin-common-api/src/attributes/output.h"
#include "../src/submodules/grin-node-api/src/attributes/blindingfactor.h"
#include "../src/submodules/grin-node-api/src/attributes/input.h"
#include "../src/submodules/grin-node-api/src/attributes/outputfeatures.h"
#include "../src/submodules/grin-node-api/src/attributes/transaction.h"
#include "../src/submodules/grin-node-api/src/attributes/transactionbody.h"
#include "../src/submodules/grin-node-api/src/attributes/txkernel.h"

namespace
{

/**
 * @brief Creates a repeated hexadecimal string for deterministic test fixtures.
 * @param nibble Hex nibble character to repeat.
 * @param bytes Number of bytes represented in the output string.
 * @return Hex string with length bytes * 2.
 */
QString repeatedHex(const char nibble, int bytes)
{
    return QString(bytes * 2, QLatin1Char(nibble));
}

/**
 * @brief Builds a deterministic keychain from a fixed mnemonic.
 * @return Test keychain instance.
 */
WalletKeychain testKeychain()
{
    static const QString mnemonic = QStringLiteral(
        "abandon abandon abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon abandon abandon abandon "
        "abandon abandon abandon abandon abandon abandon abandon art");
    return WalletKeychain(mnemonic);
}

/**
 * @brief Creates a partial Slate V4 containing sender and receiver participant data.
 * @param sender Sender participant context.
 * @param receiver Receiver participant context.
 * @return Partial slate prepared for signature workflow tests.
 */
SlateV4 buildPartialSlate(const WalletCryptoBackend::ParticipantContext &sender,
                          const WalletCryptoBackend::ParticipantContext &receiver)
{
    SlateV4 slate;
    slate.state = SlateV4::Standard2;
    slate.amount = QStringLiteral("1.000000000");
    slate.fee = QStringLiteral("0.008000000");
    slate.metadata.insert(QStringLiteral("workflow_id"), QStringLiteral("test-workflow"));
    slate.signatures.append(WalletCryptoBackend::createParticipantData(sender));
    slate.signatures.append(WalletCryptoBackend::createParticipantData(receiver));
    return slate;
}

/**
 * @brief Builds a transaction fixture containing duplicate input commitments.
 * @return Transaction used to test duplicate-input validation failures.
 */
Transaction buildTransactionWithDuplicateInputs()
{
    Commitment duplicateCommit;
    duplicateCommit.setHex(repeatedHex('1', 33));

    Input firstInput(OutputFeatures::Plain, duplicateCommit);
    Input secondInput(OutputFeatures::Plain, duplicateCommit);

    Output output(repeatedHex('2', 33), QStringLiteral("Plain"), QStringLiteral("not-used"));

    TxKernel kernel;
    kernel.setFeatures(QStringLiteral("Plain"));
    kernel.setFee(0);
    kernel.setExcess(repeatedHex('3', 33));
    kernel.setExcessSig(repeatedHex('4', 64));

    TransactionBody body;
    body.setInputs(QVector<Input>() << firstInput << secondInput);
    body.setOutputs(QVector<Output>() << output);
    body.setKernels(QVector<TxKernel>() << kernel);

    Transaction tx;
    tx.setBody(body);
    return tx;
}

/**
 * @brief Builds a transaction fixture with a single kernel signature.
 * @param excess Kernel excess commitment.
 * @param signatureHex Kernel signature encoded as hex.
 * @return Transaction containing one configured kernel.
 */
Transaction buildKernelSignatureTransaction(const QString &excess,
                                            const QString &signatureHex)
{
    TxKernel kernel;
    kernel.setFeatures(QStringLiteral("Plain"));
    kernel.setFee(0);
    kernel.setExcess(excess);
    kernel.setExcessSig(signatureHex);

    TransactionBody body;
    body.setKernels(QVector<TxKernel>() << kernel);

    Transaction tx;
    tx.setBody(body);
    return tx;
}

}

class WalletTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void paymentProofRoundTrip();
    void paymentProofRejectsWrongReceiver();
    void paymentProofDetectsTampering();
    void slatepackAddressUsesExpectedPrefixes();
    void finalizeSlateRejectsInsufficientPartialSignatures();
    void finalizeSlateSucceedsWithAllParticipants();
    void verifyPartialSignaturesRejectsMissingPartials();
    void validateTransactionBodyRejectsDuplicateInputs();
    void validateTransactionKernelSumsRejectsMissingInputs();
    void validateTransactionKernelSignaturesRejectsMalformedSignature();
};

/**
 * @brief Initializes test resources required by wallet core tests.
 */
void WalletTests::initTestCase()
{
    Q_INIT_RESOURCE(wallettestresources);
    QVERIFY(QFile::exists(QStringLiteral(":/qml/src/wallet/resources/bip39_english.txt")));
}

/**
 * @brief Verifies that payment proof signing and verification succeeds for valid data.
 */
void WalletTests::paymentProofRoundTrip()
{
    const WalletKeychain receiver = testKeychain();
    QVERIFY(receiver.isValid());

    SlateV4 slate;
    slate.amount = QStringLiteral("1.000000000");
    slate.fee = QStringLiteral("0.008000000");
    slate.hasPaymentProof = true;
    slate.paymentProof.senderAddress = QStringLiteral("test-sender");
    slate.paymentProof.receiverAddress = WalletCryptoBackend::paymentProofAddress(receiver);

    QString error;
    QVERIFY2(WalletCryptoBackend::signPaymentProof(&slate, receiver, &error), qPrintable(error));
    QVERIFY(!slate.paymentProof.receiverSignature.isEmpty());
    QVERIFY2(WalletCryptoBackend::verifyPaymentProof(slate, &error), qPrintable(error));
}

/**
 * @brief Verifies that signing fails when the receiver address does not match the keychain.
 */
void WalletTests::paymentProofRejectsWrongReceiver()
{
    const WalletKeychain receiver = testKeychain();
    QVERIFY(receiver.isValid());

    SlateV4 slate;
    slate.amount = QStringLiteral("1.000000000");
    slate.fee = QStringLiteral("0.008000000");
    slate.hasPaymentProof = true;
    slate.paymentProof.senderAddress = QStringLiteral("test-sender");
    slate.paymentProof.receiverAddress = QString(64, QLatin1Char('a'));

    QString error;
    QVERIFY(!WalletCryptoBackend::signPaymentProof(&slate, receiver, &error));
    QVERIFY(error.contains(QStringLiteral("does not match"), Qt::CaseInsensitive));
}

/**
 * @brief Verifies that payment proof verification fails after slate tampering.
 */
void WalletTests::paymentProofDetectsTampering()
{
    const WalletKeychain receiver = testKeychain();
    QVERIFY(receiver.isValid());

    SlateV4 slate;
    slate.amount = QStringLiteral("1.000000000");
    slate.fee = QStringLiteral("0.008000000");
    slate.hasPaymentProof = true;
    slate.paymentProof.senderAddress = QStringLiteral("test-sender");
    slate.paymentProof.receiverAddress = WalletCryptoBackend::paymentProofAddress(receiver);

    QString error;
    QVERIFY2(WalletCryptoBackend::signPaymentProof(&slate, receiver, &error), qPrintable(error));

    slate.amount = QStringLiteral("2.000000000");
    QVERIFY(!WalletCryptoBackend::verifyPaymentProof(slate, &error));
    QVERIFY(error.contains(QStringLiteral("verification failed"), Qt::CaseInsensitive));
}

/**
 * @brief Verifies network-specific slatepack address prefixes.
 */
void WalletTests::slatepackAddressUsesExpectedPrefixes()
{
    const WalletKeychain keychain = testKeychain();
    QVERIFY(keychain.isValid());

    const QString mainnetAddress = WalletCryptoBackend::slatepackAddress(keychain, QStringLiteral("mainnet"));
    const QString testnetAddress = WalletCryptoBackend::slatepackAddress(keychain, QStringLiteral("testnet"));

    QVERIFY(mainnetAddress.startsWith(QStringLiteral("grin1")));
    QVERIFY(testnetAddress.startsWith(QStringLiteral("tgrin1")));
    QVERIFY(mainnetAddress != testnetAddress);
}

/**
 * @brief Verifies slate finalization fails when not enough partial signatures are present.
 */
void WalletTests::finalizeSlateRejectsInsufficientPartialSignatures()
{
    const WalletCryptoBackend::ParticipantContext sender =
        WalletCryptoBackend::createRandomParticipant(QStringLiteral("sender"));
    const WalletCryptoBackend::ParticipantContext receiver =
        WalletCryptoBackend::createRandomParticipant(QStringLiteral("receiver"));

    SlateV4 slate = buildPartialSlate(sender, receiver);

    QString error;
    QVERIFY2(WalletCryptoBackend::applyRound2Signature(&slate,
                                                       QStringLiteral("unused"),
                                                       QStringLiteral("sender"),
                                                       &sender,
                                                       &error),
             qPrintable(error));
    QVERIFY(!slate.signatures.at(0).part.isEmpty() || !slate.signatures.at(1).part.isEmpty());

    QVERIFY(!WalletCryptoBackend::finalizeSlate(&slate, &error));
    QVERIFY(error.contains(QStringLiteral("Not enough partial signatures"), Qt::CaseInsensitive));
}

/**
 * @brief Verifies slate finalization succeeds after collecting all participant signatures.
 */
void WalletTests::finalizeSlateSucceedsWithAllParticipants()
{
    const WalletCryptoBackend::ParticipantContext sender =
        WalletCryptoBackend::createRandomParticipant(QStringLiteral("sender"));
    const WalletCryptoBackend::ParticipantContext receiver =
        WalletCryptoBackend::createRandomParticipant(QStringLiteral("receiver"));

    SlateV4 slate = buildPartialSlate(sender, receiver);

    QString error;
    QVERIFY2(WalletCryptoBackend::applyRound2Signature(&slate,
                                                       QStringLiteral("unused"),
                                                       QStringLiteral("sender"),
                                                       &sender,
                                                       &error),
             qPrintable(error));
    QVERIFY2(WalletCryptoBackend::applyRound2Signature(&slate,
                                                       QStringLiteral("unused"),
                                                       QStringLiteral("receiver"),
                                                       &receiver,
                                                       &error),
             qPrintable(error));
    QVERIFY2(WalletCryptoBackend::verifyPartialSignatures(slate, &error), qPrintable(error));
    QVERIFY2(WalletCryptoBackend::finalizeSlate(&slate, &error), qPrintable(error));
    QVERIFY(!slate.metadata.value(QStringLiteral("final_sig")).toString().isEmpty());
    QCOMPARE(slate.metadata.value(QStringLiteral("signature_status")).toString(),
             QStringLiteral("finalized"));
}

/**
 * @brief Verifies partial signature validation fails when no partial signatures are present.
 */
void WalletTests::verifyPartialSignaturesRejectsMissingPartials()
{
    const WalletCryptoBackend::ParticipantContext sender =
        WalletCryptoBackend::createRandomParticipant(QStringLiteral("sender"));
    const WalletCryptoBackend::ParticipantContext receiver =
        WalletCryptoBackend::createRandomParticipant(QStringLiteral("receiver"));

    SlateV4 slate = buildPartialSlate(sender, receiver);

    QString error;
    QVERIFY(!WalletCryptoBackend::verifyPartialSignatures(slate, &error));
    QVERIFY(error.contains(QStringLiteral("does not contain partial signatures"), Qt::CaseInsensitive));
}

/**
 * @brief Verifies transaction body validation rejects duplicate inputs.
 */
void WalletTests::validateTransactionBodyRejectsDuplicateInputs()
{
    const Transaction tx = buildTransactionWithDuplicateInputs();

    QString error;
    QVERIFY(!WalletCryptoBackend::validateTransactionBody(tx, &error));
    QVERIFY(error.contains(QStringLiteral("duplicate input"), Qt::CaseInsensitive));
}

/**
 * @brief Verifies kernel-sum validation fails for transactions without inputs.
 */
void WalletTests::validateTransactionKernelSumsRejectsMissingInputs()
{
    Transaction tx;

    QString error;
    QVERIFY(!WalletCryptoBackend::validateTransactionKernelSums(tx, &error));
    QVERIFY(error.contains(QStringLiteral("no inputs"), Qt::CaseInsensitive));
}

/**
 * @brief Verifies kernel-signature validation rejects malformed signatures.
 */
void WalletTests::validateTransactionKernelSignaturesRejectsMalformedSignature()
{
    const WalletCryptoBackend::CommitmentResult commitment =
        WalletCryptoBackend::createCommitment(QStringLiteral("test-fingerprint"),
                                              QStringLiteral("test-kernel"),
                                              QStringLiteral("sender"),
                                              QStringLiteral("1.000000000"));
    QVERIFY(commitment.success);

    const Transaction tx = buildKernelSignatureTransaction(commitment.commit.commitment,
                                                           QStringLiteral("00"));

    QString error;
    QVERIFY(!WalletCryptoBackend::validateTransactionKernelSignatures(tx, &error));
    QVERIFY(error.contains(QStringLiteral("invalid signature length"), Qt::CaseInsensitive));
}

/**
 * @brief Creates the wallet core test object for the shared test runner.
 * @return Newly allocated Qt test object instance.
 */
QObject *createWalletCoreTests()
{
    return new WalletTests;
}

#include "tst_walletcore.moc"
