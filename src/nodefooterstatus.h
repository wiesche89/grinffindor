#ifndef NODEFOOTERSTATUS_H
#define NODEFOOTERSTATUS_H

#include <QObject>
#include <QTimer>

#include "nodeforeignapi.h"

class NodeFooterStatus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool mainnetAvailable READ mainnetAvailable NOTIFY dataChanged)
    Q_PROPERTY(QString mainnetTip READ mainnetTip NOTIFY dataChanged)
    Q_PROPERTY(QString mainnetVersion READ mainnetVersion NOTIFY dataChanged)
    Q_PROPERTY(bool testnetAvailable READ testnetAvailable NOTIFY dataChanged)
    Q_PROPERTY(QString testnetTip READ testnetTip NOTIFY dataChanged)
    Q_PROPERTY(QString testnetVersion READ testnetVersion NOTIFY dataChanged)

public:
    explicit NodeFooterStatus(QObject *parent = nullptr);

    bool mainnetAvailable() const;
    QString mainnetTip() const;
    QString mainnetVersion() const;
    bool testnetAvailable() const;
    QString testnetTip() const;
    QString testnetVersion() const;

    Q_INVOKABLE void refresh();

signals:
    void dataChanged();

private:
    void connectApi(NodeForeignApi *api, bool &availableTarget, QString &tipTarget, QString &versionTarget);
    static QString formatTip(const Tip &tip);
    static QString formatVersion(const NodeVersion &version);

    bool m_mainnetAvailable{false};
    QString m_mainnetTip{"..."};
    QString m_mainnetVersion{"..."};
    bool m_testnetAvailable{false};
    QString m_testnetTip{"..."};
    QString m_testnetVersion{"..."};

    NodeForeignApi m_mainnetApi;
    NodeForeignApi m_testnetApi;
    QTimer m_refreshTimer;
};

#endif
