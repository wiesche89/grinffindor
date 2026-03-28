#ifndef BINARYSLATEV4WRITER_H
#define BINARYSLATEV4WRITER_H

#include <QString>
#include <QStringList>

class SlateV4;

class BinarySlateV4Writer
{
public:
    static bool encodeSlatepack(const SlateV4 &slate,
                                QString *armoredOut,
                                QString *errorOut = 0,
                                const QString &sender = QString(),
                                const QStringList &recipients = QStringList(),
                                const QByteArray &senderSecret = QByteArray());
};

#endif // BINARYSLATEV4WRITER_H
