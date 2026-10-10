#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include "displayjson.h"
#include "tst_nodeblock.h"
#include "nodeforeignapi.h"

void NodeBlockTests::exactIntegers()
{
    const QByteArray input = R"({"nonce":18446744073709551615,"negative":-9223372036854775808,"height":3286802,"fraction":1234567890123456.5,"exponent":1234567890123456e2,"text":"escaped \"18446744073709551615\""})";
    QJsonParseError error;
    const auto value = QJsonDocument::fromJson(displayJson(input), &error).object();
    QCOMPARE(error.error, QJsonParseError::NoError);
    QCOMPARE(value["nonce"].toString(), QStringLiteral("18446744073709551615"));
    QCOMPARE(value["negative"].toString(), QStringLiteral("-9223372036854775808"));
    QCOMPARE(value["height"].toInt(), 3286802);
    QVERIFY(value["fraction"].isDouble());
    QVERIFY(value["exponent"].isDouble());
    QCOMPARE(value["text"].toString(), QStringLiteral("escaped \"18446744073709551615\""));
    QJsonDocument::fromJson(displayJson(R"({"invalid":01234567890123456})"), &error);
    QVERIFY(error.error != QJsonParseError::NoError);
}

void NodeBlockTests::blockResponse_data()
{
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<bool>("valid");
    QTest::newRow("block") << QByteArray(R"({"jsonrpc":"2.0","id":1,"result":{"Ok":{"header":{"nonce":18446744073709551615},"outputs":[]}}})") << true;
    QTest::newRow("rpc-error") << QByteArray(R"({"jsonrpc":"2.0","id":1,"error":{"code":-32603}})") << false;
    QTest::newRow("wrong-id") << QByteArray(R"({"jsonrpc":"2.0","id":2,"result":{"Ok":{}}})") << false;
    QTest::newRow("malformed") << QByteArray("{") << false;
}

void NodeBlockTests::blockResponse()
{
    QFETCH(QByteArray, body);
    QFETCH(bool, valid);
    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    connect(&server, &QTcpServer::newConnection, &server, [&] {
        auto *socket = server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, socket, [socket, body] {
            socket->readAll();
            socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                          + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
            socket->disconnectFromHost();
        });
    });
    NodeForeignApi api(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort()), {});
    QSignalSpy spy(&api, &NodeForeignApi::blockForDisplayFinished);
    api.getBlockForDisplayAsync(42);
    QVERIFY(spy.wait());
    const auto result = spy.takeFirst();
    QCOMPARE(result[1].toString().isEmpty(), valid);
    if (valid)
        QCOMPARE(result[0].toJsonObject()["header"].toObject()["nonce"].toString(), QStringLiteral("18446744073709551615"));
}
QTEST_GUILESS_MAIN(NodeBlockTests)
