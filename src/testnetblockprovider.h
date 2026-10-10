#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QJsonObject>

class NodeForeignApi;

class TestnetBlockProvider : public QObject
{
    Q_OBJECT
public:
    explicit TestnetBlockProvider(QObject *parent = nullptr);
    Q_INVOKABLE int nextRequestId() { return ++m_nextId; }
    Q_INVOKABLE void lookup(const QString &kernel, int id, int height);
    Q_INVOKABLE void cancel(int id);

signals:
    void finished(int id, const QString &blockJson, const QString &error);

private:
    void complete(const QJsonObject &block, const QString &error);
    QPointer<NodeForeignApi> m_api;
    QTimer m_timeout;
    int m_id = 0;
    int m_nextId = 0;
};
