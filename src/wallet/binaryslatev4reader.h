#ifndef BINARYSLATEV4READER_H
#define BINARYSLATEV4READER_H

#include <QByteArray>
#include <QString>

class BinarySlateV4Reader
{
public:
    /**
     * @brief Decodes a binary slatepack payload into slate JSON text.
     * @param payload
     * @param decryptionKey
     * @param decodedOut
     * @param errorOut
     * @return
     */
    static bool decodeSlatepackPayload(const QByteArray &payload,
                                       const QByteArray &decryptionKey,
                                       QString *decodedOut,
                                       QString *errorOut);
};

#endif // BINARYSLATEV4READER_H
