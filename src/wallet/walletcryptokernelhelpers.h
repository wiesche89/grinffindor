#ifndef WALLETCRYPTOKERNELHELPERS_H
#define WALLETCRYPTOKERNELHELPERS_H

#include <QString>

class Transaction;
class Input;
class Output;
class TxKernel;

namespace WalletCryptoKernelHelpers
{
/**
 * @brief Returns input order hash.
 */
QString inputOrderHash(const Input &input);
/**
 * @brief Returns output order hash.
 */
QString outputOrderHash(const Output &output);
/**
 * @brief Returns kernel order hash.
 */
QString kernelOrderHash(const TxKernel &kernel);
/**
 * @brief Validates validate transaction body.
 */
bool validateTransactionBody(const Transaction &tx, QString *errorOut);
/**
 * @brief Validates validate transaction kernel sums.
 */
bool validateTransactionKernelSums(const Transaction &tx, QString *errorOut);
/**
 * @brief Validates validate transaction kernel signatures.
 */
bool validateTransactionKernelSignatures(const Transaction &tx, QString *errorOut);
}

#endif // WALLETCRYPTOKERNELHELPERS_H
