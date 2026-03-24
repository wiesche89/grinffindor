#ifndef WALLETOUTPUT_H
#define WALLETOUTPUT_H

#include <QJsonObject>
#include <QString>

class WalletOutput
{
public:
    QString commitment;
    QString proof;
    QString amount;
    QString source;
    QString keyPath;
    QString blindingFactor;
    quint32 childIndex = 0;
    quint64 height = 0;
    bool coinbase = false;
    bool onChain = false;
    bool spent = false;
    bool locked = false;
    bool pending = false;
    QString workflowId;

    QJsonObject toJson() const;
    static WalletOutput fromJson(const QJsonObject &json);
};

#endif // WALLETOUTPUT_H
