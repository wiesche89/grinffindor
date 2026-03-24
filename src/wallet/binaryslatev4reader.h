#ifndef BINARYSLATEV4READER_H
#define BINARYSLATEV4READER_H

#include <QByteArray>
#include <QString>

class BinarySlateV4Reader
{
public:
    static bool decodeSlatepackPayload(const QByteArray &payload, QString *decodedOut, QString *errorOut);
};

#endif // BINARYSLATEV4READER_H
