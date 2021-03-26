/**
 * Copyright (c) 2012-2014 Luke Dashjr
 * Copyright (c) 2013-2014 Pavol Rusnak
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <btc/base58.h>

#include <string.h>
#include <sys/types.h>

#include <btc/chainparams.h>
#include <btc/memory.h>
#include <btc/segwit_addr.h>
#include <btc/sha2.h>

static const int8_t b58digits_map[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, -1, -1, -1, -1, -1, -1,
    -1, 9, 10, 11, 12, 13, 14, 15, 16, -1, 17, 18, 19, 20, 21, -1,
    22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, -1, -1, -1, -1, -1,
    -1, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, -1, 44, 45, 46,
    47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, -1, -1, -1, -1, -1,
};

int btc_base58_decode(void* bin, size_t* binszp, const char* b58)
{
    size_t binsz = *binszp;
    const unsigned char* b58u = (const void*)b58;
    unsigned char* binu = bin;
    const size_t outisz = (binsz + 3) / 4;
    const size_t outilen = sizeof(uint32_t) * outisz;
    uint32_t *outi = (uint32_t *)btc_malloc(outilen);
    uint64_t t;
    uint32_t c;
    size_t i, j;
    uint8_t bytesleft = binsz % 4;
    uint32_t zeromask = bytesleft ? (0xffffffff << (bytesleft * 8)) : 0;
    unsigned zerocount = 0;
    size_t b58sz;

    b58sz = strlen(b58);

    memset(outi, 0, outilen);

    // Leading zeros, just count
    for (i = 0; i < b58sz && !b58digits_map[b58u[i]]; ++i) {
        ++zerocount;
    }

    for (; i < b58sz; ++i) {
        if (b58u[i] & 0x80) {
            // High-bit set on invalid digit
            btc_free(outi);
            return false;
        }
        if (b58digits_map[b58u[i]] == -1) {
            // Invalid base58 digit
            btc_free(outi);
            return false;
        }
        c = (unsigned)b58digits_map[b58u[i]];
        for (j = outisz; j--;) {
            t = ((uint64_t)outi[j]) * 58 + c;
            c = (t & 0x3f00000000) >> 32;
            outi[j] = t & 0xffffffff;
        }
        if (c) {
            // Output number too big (carry to the next int32)
            memset(outi, 0, outilen);
            btc_free(outi);
            return false;
        }
        if (outi[0] & zeromask) {
            // Output number too big (last int32 filled too far)
            memset(outi, 0, outilen);
            btc_free(outi);
            return false;
        }
    }

    j = 0;
    switch (bytesleft) {
    case 3:
        *(binu++) = (outi[0] & 0xff0000) >> 16;
    case 2:
        *(binu++) = (outi[0] & 0xff00) >> 8;
    case 1:
        *(binu++) = (outi[0] & 0xff);
        ++j;
    default:
        break;
    }

    for (; j < outisz; ++j) {
        *(binu++) = (outi[j] >> 0x18) & 0xff;
        *(binu++) = (outi[j] >> 0x10) & 0xff;
        *(binu++) = (outi[j] >> 8) & 0xff;
        *(binu++) = (outi[j] >> 0) & 0xff;
    }

    // Count canonical base58 byte count
    binu = bin;
    for (i = 0; i < binsz; ++i) {
        if (binu[i]) {
            break;
        }
        --*binszp;
    }
    *binszp += zerocount;

    memset(outi, 0, outilen);
    btc_free(outi);
    return true;
}

int btc_b58check(const void* bin, size_t binsz, const char* base58str)
{
    uint256 buf;
    const uint8_t* binc = bin;
    unsigned i;
    if (binsz < 4) {
        return -4;
    }
    sha256_Raw(bin, binsz - 4, buf);
    sha256_Raw(buf, sizeof(buf), buf);
    if (memcmp(&binc[binsz - 4], buf, 4)) {
        return -1;
    }

    // Check number of zeros is correct AFTER verifying checksum (to avoid possibility of accessing base58str beyond the end)
    for (i = 0; binc[i] == '\0' && base58str[i] == '1'; ++i) {
    } // Just finding the end of zeros, nothing to do in loop
    if (binc[i] == '\0' || base58str[i] == '1') {
        return -3;
    }

    return binc[0];
}

static const char b58digits_ordered[] =
    "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

int btc_base58_encode(char* b58, size_t* b58sz, const void* data, size_t binsz)
{
    const uint8_t* bin = data;
    int carry;
    ssize_t i, j = 0, high, zcount = 0;

    while (zcount < (ssize_t)binsz && !bin[zcount]) {
        ++zcount;
    }

    const size_t size = (binsz - zcount) * 138 / 100 + 1;
    uint8_t *buf = (uint8_t *)btc_malloc(size);
    memset(buf, 0, size);

    for (i = zcount, high = size - 1; i < (ssize_t)binsz; ++i, high = j) {
        for (carry = bin[i], j = size - 1; (j > high) || carry; --j) {
            carry += 256 * buf[j];
            buf[j] = carry % 58;
            carry /= 58;
        }
    }

    for (j = 0; j < (ssize_t)size && !buf[j]; ++j)
        ;

    if (*b58sz <= zcount + size - j) {
        *b58sz = zcount + size - j + 1;
        memset(buf, 0, size);
        btc_free(buf);
        return false;
    }

    if (zcount) {
        memset(b58, '1', zcount);
    }
    for (i = zcount; j < (ssize_t)size; ++i, ++j) {
        b58[i] = b58digits_ordered[buf[j]];
    }
    b58[i] = '\0';
    *b58sz = i + 1;

    memset(buf, 0, size);
    btc_free(buf);
    return true;
}

int btc_base58_encode_check(const uint8_t* data, int datalen, char* str, int strsize)
{
    int ret;
    if (datalen > 128) {
        return 0;
    }
    uint8_t buf[128 + 32];
    uint8_t* hash = buf + datalen;
    memcpy(buf, data, datalen);
    sha256_Raw(data, datalen, hash);
    sha256_Raw(hash, 32, hash);
    size_t res = strsize;
    if (btc_base58_encode(str, &res, buf, datalen + 4) != true) {
        ret = 0;
    } else {
        ret = (int)res;
    }
    memset(buf, 0, datalen);
    return ret;
}

int btc_base58_decode_check(const char* str, uint8_t* data, size_t datalen)
{
    int ret;
    size_t strl = strlen(str);

    /* buffer needs to be at least the strsize, will be used
       for the whole decoding */
    if (strl > 128 || datalen < strl) {
        return 0;
    }

    size_t binsize = strl;
    if (btc_base58_decode(data, &binsize, str) != true) {
        ret = 0;
    }

    memmove(data, data + strl - binsize, binsize);
    memset(data + binsize, 0, datalen - binsize);

    if (btc_b58check(data, binsize, str) < 0) {
        ret = 0;
    } else {
        ret = (int)binsize;
    }
    return ret;
}

btc_bool btc_p2pkh_addr_from_hash160(const uint160 hashin, const btc_chainparams* chain, char *addrout, int len) {
    uint8_t hash160[sizeof(uint160)+1];
    hash160[0] = chain->b58prefix_pubkey_address;
    memcpy(hash160 + 1, hashin, sizeof(uint160));

    return (btc_base58_encode_check(hash160, sizeof(uint160)+1, addrout, len) > 0);
}

btc_bool btc_p2sh_addr_from_hash160(const uint160 hashin, const btc_chainparams* chain, char* addrout,
                                    int len)
{
    uint8_t hash160[sizeof(uint160) + 1];
    hash160[0] = chain->b58prefix_script_address;
    memcpy(hash160 + 1, hashin, sizeof(uint160));

    return (btc_base58_encode_check(hash160, sizeof(uint160) + 1, addrout, len) > 0);
}

btc_bool btc_p2wpkh_addr_from_hash160(const uint160 hashin, const btc_chainparams* chain, char *addrout) {
    return segwit_addr_encode(addrout, chain->bech32_hrp, 0, hashin, sizeof(uint160));
}
