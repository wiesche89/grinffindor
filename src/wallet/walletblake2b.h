#ifndef WALLETBLAKE2B_H
#define WALLETBLAKE2B_H

#include <QByteArray>

namespace WalletBlake2b {

/**
 * @brief Returns whether h256.
 */
QByteArray hash256(const QByteArray &data);
/**
 * @brief Returns whether h256.
 */
QByteArray hash256(const QByteArray &key, const QByteArray &data);

}

#endif // WALLETBLAKE2B_H
