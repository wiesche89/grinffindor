#pragma once
#include <QObject>

class NodeBlockTests : public QObject
{
    Q_OBJECT
private slots:
    void exactIntegers();
    void blockResponse_data();
    void blockResponse();
};
