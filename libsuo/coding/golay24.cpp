/* -*- c -*- */
/*
 * Copyright 2017 Daniel Estevez <daniel@destevez.net>
 *
 * This file is part of gr-satellites
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 */

/*
 This algorithm is based on
 R.H. Morelos-Zaragoza, The Art of Error Correcting Coding, Wiley, 2002; Section 2.2.3
*/

#include "golay24.hpp"

#include <bit> // std::popcount
#include "framing/utils.hpp" // suo::bit_parity


#define N 12

static const uint32_t H[N] = { 0x8008ed, 0x4001db, 0x2003b5, 0x100769, 0x80ed1, 0x40da3,
                               0x20b47,  0x1068f,  0x8d1d,   0x4a3b,   0x2477,  0x1ffe };

#define B(i) (H[i] & 0xfff)

int encode_golay24(uint32_t* data)
{
    uint32_t r = (*data) & 0xfff;
    uint32_t s;
    int i;

    s = 0;
    for (i = 0; i < N; i++) {
        s <<= 1;
        s |= suo::bit_parity(H[i] & r);
    }

    *data = ((0xFFF & s) << N) | r;
    return 0;
}

int decode_golay24(uint32_t* data)
{
    uint32_t r = *data;
    uint16_t s; /* syndrome */
    uint16_t q; /* modified syndrome */
    uint32_t e; /* estimated error vector */
    int i;
    uint32_t count;

    // Step 1. s = H*r
    s = 0;
    for (i = 0; i < N; i++) {
        s <<= 1;
        s |= suo::bit_parity(H[i] & r);
    }

    // Step 2. if w(s) <= 3, then e = (s, 0) and go to step 8
    count = std::popcount(s);
    if (count <= 3) {
        e = s;
        e <<= N;
        goto step8;
    }

    // Step 3. if w(s + B[i]) <= 2, then e = (s + B[i], e_{i+1}) and go to step 8
    for (i = 0; i < N; i++) {
        count = std::popcount(s ^ B(i));
        if (count <= 2) {
            e = s ^ B(i);
            e <<= N;
            e |= 1 << (N - i - 1);
            goto step8;
        }
    }

    // Step 4. compute q = B*s
    q = 0;
    for (i = 0; i < N; i++) {
        q <<= 1;
        q |= suo::bit_parity(B(i) & s);
    }

    // Step 5. If w(q) <= 3, then e = (0, q) and go to step 8
    count = std::popcount(q);
    if (count <= 3) {
        e = q;
        goto step8;
    }

    // Step 6. If w(q + B[i]) <= 2, then e = (e_{i+1}, q + B[i]) and got to step 8
    for (i = 0; i < N; i++) {
        count = std::popcount(q ^ B(i));
        if (count <= 2) {
            e = 1 << (2 * N - i - 1);
            e |= q ^ B(i);
            goto step8;
        }
    }

    // Step 7. r is uncorrectable
    return -1;

step8:
    // Step 8. c = r + e
    *data = r ^ e;

    count = std::popcount(e);
    return count;
}
