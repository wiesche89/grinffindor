#ifndef BINARYSLATEV4WRITER_H
#define BINARYSLATEV4WRITER_H

#include <QString>

class SlateV4;

class BinarySlateV4Writer
{
public:
    static bool encodeSlatepack(const SlateV4 &slate, QString *armoredOut, QString *errorOut = 0);
};

#endif // BINARYSLATEV4WRITER_H
