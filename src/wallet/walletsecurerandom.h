#ifndef WALLETSECURERANDOM_H
#define WALLETSECURERANDOM_H

#include <QByteArray>

class WalletSecureRandom
{
public:
    static bool fill(QByteArray *buffer);
    static QByteArray bytes(int size);
    static bool selfTest();
};

#endif // WALLETSECURERANDOM_H
