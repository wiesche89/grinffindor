#ifndef WALLETCRYPTOKERNELHELPERS_H
#define WALLETCRYPTOKERNELHELPERS_H

#include <QString>

class Transaction;
class Input;
class Output;
class TxKernel;

namespace WalletCryptoKernelHelpers
{
QString inputOrderHash(const Input &input);
QString outputOrderHash(const Output &output);
QString kernelOrderHash(const TxKernel &kernel);
bool validateTransactionBody(const Transaction &tx, QString *errorOut);
bool validateTransactionKernelSums(const Transaction &tx, QString *errorOut);
bool validateTransactionKernelSignatures(const Transaction &tx, QString *errorOut);
}

#endif // WALLETCRYPTOKERNELHELPERS_H
