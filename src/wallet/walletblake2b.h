#ifndef WALLETBLAKE2B_H
#define WALLETBLAKE2B_H

#include <QByteArray>

namespace WalletBlake2b {

QByteArray hash256(const QByteArray &data);
QByteArray hash256(const QByteArray &key, const QByteArray &data);

}

#endif // WALLETBLAKE2B_H
