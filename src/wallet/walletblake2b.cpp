#include "walletblake2b.h"

#include <QtGlobal>
#include <cstring>

namespace {

struct Blake2bState
{
    quint64 h[8];
    quint64 t[2];
    quint64 f[2];
    unsigned char buf[128];
    size_t buflen = 0;
    size_t outlen = 0;
};

static const quint64 kIv[8] = {
    Q_UINT64_C(0x6a09e667f3bcc908), Q_UINT64_C(0xbb67ae8584caa73b),
    Q_UINT64_C(0x3c6ef372fe94f82b), Q_UINT64_C(0xa54ff53a5f1d36f1),
    Q_UINT64_C(0x510e527fade682d1), Q_UINT64_C(0x9b05688c2b3e6c1f),
    Q_UINT64_C(0x1f83d9abfb41bd6b), Q_UINT64_C(0x5be0cd19137e2179)
};

static const unsigned char kSigma[12][16] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15 },
    {14,10, 4, 8, 9,15,13, 6, 1,12, 0, 2,11, 7, 5, 3 },
    {11, 8,12, 0, 5, 2,15,13,10,14, 3, 6, 7, 1, 9, 4 },
    { 7, 9, 3, 1,13,12,11,14, 2, 6, 5,10, 4, 0,15, 8 },
    { 9, 0, 5, 7, 2, 4,10,15,14, 1,11,12, 6, 8, 3,13 },
    { 2,12, 6,10, 0,11, 8, 3, 4,13, 7, 5,15,14, 1, 9 },
    {12, 5, 1,15,14,13, 4,10, 0, 7, 6, 3, 9, 2, 8,11 },
    {13,11, 7,14,12, 1, 3, 9, 5, 0,15, 4, 8, 6, 2,10 },
    { 6,15,14, 9,11, 3, 0, 8,12, 2,13, 7, 1, 4,10, 5 },
    {10, 2, 8, 4, 7, 6, 1, 5,15,11, 9,14, 3,12,13, 0 },
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15 },
    {14,10, 4, 8, 9,15,13, 6, 1,12, 0, 2,11, 7, 5, 3 }
};

/**
 * @brief Rotates a 64-bit value to the right by the provided shift.
 * @param x
 * @param c
 * @return
 */
quint64 rotr64(const quint64 x, const unsigned int c)
{
    return (x >> c) | (x << (64U - c));
}

/**
 * @brief Loads a little-endian 64-bit value from a byte pointer.
 * @param src
 * @return
 */
quint64 load64(const unsigned char *src)
{
    quint64 value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= (static_cast<quint64>(src[i]) << (8 * i));
    }
    return value;
}

/**
 * @brief Stores a 64-bit value as little-endian bytes.
 * @param dst
 * @param value
 */
void store64(unsigned char *dst, quint64 value)
{
    for (int i = 0; i < 8; ++i) {
        dst[i] = static_cast<unsigned char>((value >> (8 * i)) & 0xff);
    }
}

/**
 * @brief Applies one Blake2b G mixing round to the working state.
 * @param a
 * @param b
 * @param c
 * @param d
 * @param x
 * @param y
 */
void g(quint64 &a, quint64 &b, quint64 &c, quint64 &d, quint64 x, quint64 y)
{
    a = a + b + x;
    d = rotr64(d ^ a, 32);
    c = c + d;
    b = rotr64(b ^ c, 24);
    a = a + b + y;
    d = rotr64(d ^ a, 16);
    c = c + d;
    b = rotr64(b ^ c, 63);
}

/**
 * @brief Compresses a Blake2b message block into the running hash state.
 * @param state
 * @param block[128
 */
void compress(Blake2bState *state, const unsigned char block[128])
{
    quint64 m[16];
    quint64 v[16];

    for (int i = 0; i < 16; ++i) {
        m[i] = load64(block + (i * 8));
    }

    for (int i = 0; i < 8; ++i) {
        v[i] = state->h[i];
        v[i + 8] = kIv[i];
    }

    v[12] ^= state->t[0];
    v[13] ^= state->t[1];
    v[14] ^= state->f[0];
    v[15] ^= state->f[1];

    for (int round = 0; round < 12; ++round) {
        g(v[0], v[4], v[8],  v[12], m[kSigma[round][0]],  m[kSigma[round][1]]);
        g(v[1], v[5], v[9],  v[13], m[kSigma[round][2]],  m[kSigma[round][3]]);
        g(v[2], v[6], v[10], v[14], m[kSigma[round][4]],  m[kSigma[round][5]]);
        g(v[3], v[7], v[11], v[15], m[kSigma[round][6]],  m[kSigma[round][7]]);
        g(v[0], v[5], v[10], v[15], m[kSigma[round][8]],  m[kSigma[round][9]]);
        g(v[1], v[6], v[11], v[12], m[kSigma[round][10]], m[kSigma[round][11]]);
        g(v[2], v[7], v[8],  v[13], m[kSigma[round][12]], m[kSigma[round][13]]);
        g(v[3], v[4], v[9],  v[14], m[kSigma[round][14]], m[kSigma[round][15]]);
    }

    for (int i = 0; i < 8; ++i) {
        state->h[i] ^= v[i] ^ v[i + 8];
    }
}

/**
 * @brief Processes increment counter.
 * @param state
 * @param inc
 */
void incrementCounter(Blake2bState *state, quint64 inc)
{
    state->t[0] += inc;
    if (state->t[0] < inc) {
        state->t[1] += 1;
    }
}

/**
 * @brief Initializes Blake2b hashing state with the requested output length.
 * @param state
 * @param outlen
 * @param key
 * @param keylen
 * @return
 */
bool init(Blake2bState *state, size_t outlen, const unsigned char *key, size_t keylen)
{
    if (!state || outlen == 0 || outlen > 64 || keylen > 64) {
        return false;
    }

    *state = Blake2bState();
    for (int i = 0; i < 8; ++i) {
        state->h[i] = kIv[i];
    }

    state->outlen = outlen;
    state->h[0] ^= Q_UINT64_C(0x01010000) ^ (static_cast<quint64>(keylen) << 8) ^ static_cast<quint64>(outlen);

    if (key && keylen > 0) {
        unsigned char block[128];
        std::memset(block, 0, sizeof(block));
        std::memcpy(block, key, keylen);
        std::memcpy(state->buf, block, sizeof(block));
        state->buflen = sizeof(block);
    }

    return true;
}

/**
 * @brief Updates Blake2b state with additional message bytes.
 * @param state
 * @param data
 * @param len
 */
void update(Blake2bState *state, const unsigned char *data, size_t len)
{
    if (!state || !data || len == 0) {
        return;
    }

    size_t left = state->buflen;
    size_t fill = 128 - left;

    if (left > 0 && len > fill) {
        std::memcpy(state->buf + left, data, fill);
        incrementCounter(state, 128);
        compress(state, state->buf);
        state->buflen = 0;
        data += fill;
        len -= fill;
    }

    while (len > 128) {
        incrementCounter(state, 128);
        compress(state, data);
        data += 128;
        len -= 128;
    }

    std::memcpy(state->buf + state->buflen, data, len);
    state->buflen += len;
}

/**
 * @brief Finalizes Blake2b state and writes the output digest.
 * @param state
 * @return
 */
QByteArray finalize(Blake2bState *state)
{
    QByteArray out(static_cast<qsizetype>(state->outlen), Qt::Uninitialized);
    unsigned char buffer[64];

    incrementCounter(state, state->buflen);
    state->f[0] = ~Q_UINT64_C(0);
    std::memset(state->buf + state->buflen, 0, 128 - state->buflen);
    compress(state, state->buf);

    for (int i = 0; i < 8; ++i) {
        store64(buffer + sizeof(quint64) * i, state->h[i]);
    }

    std::memcpy(out.data(), buffer, state->outlen);
    std::memset(buffer, 0, sizeof(buffer));
    return out;
}

}

/**
 * @brief WalletBlake2b::hash256
 * @param data
 * @return
 */
QByteArray WalletBlake2b::hash256(const QByteArray &data)
{
    Blake2bState state;
    if (!init(&state, 32, nullptr, 0)) {
        return QByteArray();
    }
    update(&state,
           reinterpret_cast<const unsigned char *>(data.constData()),
           static_cast<size_t>(data.size()));
    return finalize(&state);
}

/**
 * @brief WalletBlake2b::hash256
 * @param key
 * @param data
 * @return
 */
QByteArray WalletBlake2b::hash256(const QByteArray &key, const QByteArray &data)
{
    Blake2bState state;
    if (!init(&state,
              32,

              reinterpret_cast<const unsigned char *>(key.constData()),
              static_cast<size_t>(key.size()))) {
        return QByteArray();
    }
    update(&state,
           reinterpret_cast<const unsigned char *>(data.constData()),
           static_cast<size_t>(data.size()));
    return finalize(&state);
}
