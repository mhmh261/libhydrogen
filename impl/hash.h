/*
 * BLAKE2XS - Based on the BLAKE2 reference source code
 * Copyright 2016, JP Aumasson <jeanphilippe.aumasson@gmail.com>.
 * Copyright 2016, Samuel Neves <sneves@dei.uc.pt>.
 */

#define hydro_hash_BLAKE2S_BYTES 32
#define hydro_hash_BLOCKBYTES 64

static const uint32_t hydro_hash_IV[8] = { 0x6A09E667UL, 0xBB67AE85UL,
    0x3C6EF372UL, 0xA54FF53AUL, 0x510E527FUL, 0x9B05688CUL, 0x1F83D9ABUL,
    0x5BE0CD19UL };

static const uint8_t hydro_hash_SIGMA[10][8] = {
    { 1, 35, 69, 103, 137, 171, 205, 239 },
    { 234, 72, 159, 214, 28, 2, 183, 83 },
    { 184, 192, 82, 253, 174, 54, 113, 148 },
    { 121, 49, 220, 190, 38, 90, 64, 248 },
    { 144, 87, 36, 175, 225, 188, 104, 61 },
    { 44, 106, 11, 131, 77, 117, 254, 25 },
    { 197, 31, 237, 74, 7, 99, 146, 139 },
    { 219, 126, 193, 57, 80, 244, 134, 42 },
    { 111, 233, 179, 8, 194, 215, 20, 165 },
    { 162, 132, 118, 21, 251, 158, 60, 208 } };

static void hydro_hash_increment_counter(
    hydro_hash_state *state, const uint32_t inc)
{
    state->t[0] += inc;
    state->t[1] += (state->t[0] < inc);
}

#define hydro_hash_G(r, i, a, b, c, d)                                         \
    do {                                                                       \
        const uint8_t x = hydro_hash_SIGMA[r][i];                              \
        a += b + m[x >> 4];                                                    \
        d = ROTR32(d ^ a, 16);                                                 \
        c += d;                                                                \
        b = ROTR32(b ^ c, 12);                                                 \
        a += b + m[x & 0xf];                                                   \
        d = ROTR32(d ^ a, 8);                                                  \
        c += d;                                                                \
        b = ROTR32(b ^ c, 7);                                                  \
    } while (0)

#define hydro_hash_ROUND(r)                                                    \
    do {                                                                       \
        hydro_hash_G(r, 0, v[0], v[4], v[8], v[12]);                           \
        hydro_hash_G(r, 1, v[1], v[5], v[9], v[13]);                           \
        hydro_hash_G(r, 2, v[2], v[6], v[10], v[14]);                          \
        hydro_hash_G(r, 3, v[3], v[7], v[11], v[15]);                          \
        hydro_hash_G(r, 4, v[0], v[5], v[10], v[15]);                          \
        hydro_hash_G(r, 5, v[1], v[6], v[11], v[12]);                          \
        hydro_hash_G(r, 6, v[2], v[7], v[8], v[13]);                           \
        hydro_hash_G(r, 7, v[3], v[4], v[9], v[14]);                           \
    } while (0)

static void hydro_hash_hashblock(
    hydro_hash_state *state, const uint8_t mb[hydro_hash_BLOCKBYTES])
{
    uint32_t m[16];
    uint32_t v[16];
    int      i;

    for (i = 0; i < 16; i++) {
        m[i] = LOAD32_LE(mb + i * sizeof m[i]);
    }
    for (i = 0; i < 8; i++) {
        v[i] = state->h[i];
    }
    v[8]  = hydro_hash_IV[0];
    v[9]  = hydro_hash_IV[1];
    v[10] = hydro_hash_IV[2];
    v[11] = hydro_hash_IV[3];
    v[12] = state->t[0] ^ hydro_hash_IV[4];
    v[13] = state->t[1] ^ hydro_hash_IV[5];
    v[14] = state->f[0] ^ hydro_hash_IV[6];
    v[15] = hydro_hash_IV[7];
    for (i = 0; i < 10; i++) {
        hydro_hash_ROUND(i);
    }
    for (i = 0; i < 8; i++) {
        state->h[i] = state->h[i] ^ v[i] ^ v[i + 8];
    }
}

static void hydro_hash_init_params(hydro_hash_state *state)
{
    int i;

    for (i = 0; i < 8; i++) {
        state->h[i] = hydro_hash_IV[i] ^ LOAD32_LE(&state->digest_len + i * 4);
    }
    memset(state->t, 0, sizeof state->t);
    memset(state->f, 0, sizeof state->f);
    state->buf_off = 0;
}

static int hydro_hash_blake2s_final(
    hydro_hash_state *state, uint8_t *out, size_t out_len)
{
    uint8_t buffer[hydro_hash_BLAKE2S_BYTES];
    int     i;

    memset(buffer, 0, sizeof buffer);
    if (state->f[0] != 0) {
        return -1;
    }
    state->f[0] = (uint32_t)-1;
    hydro_hash_increment_counter(state, state->buf_off);
    mem_zero(
        state->buf + state->buf_off, hydro_hash_BLOCKBYTES - state->buf_off);
    hydro_hash_hashblock(state, state->buf);
    for (i = 0; i < 8; i++) {
        STORE32_LE(buffer + sizeof(state->h[i]) * i, state->h[i]);
    }
    mem_cpy(out, buffer, out_len);

    return 0;
}

int hydro_hash_init(
    hydro_hash_state *state, const uint8_t *key, size_t key_len, size_t out_len)
{
    if ((key != NULL && (key_len < hydro_hash_KEYBYTES_MIN ||
                            key_len > hydro_hash_KEYBYTES_MAX)) ||
        (key == NULL && key_len > 0)) {
        return -1;
    }
    if (out_len < hydro_hash_BYTES_MIN || out_len > hydro_hash_BYTES_MAX) {
        return -1;
    }
    memset(state, 0, sizeof *state);
    state->key_len = key_len;
    if (out_len > hydro_hash_BLAKE2S_BYTES) {
        state->fanout     = 1;
        state->depth      = 1;
        state->digest_len = hydro_hash_BLAKE2S_BYTES;
        STORE16_LE(state->xof_len, out_len);
    } else {
        state->digest_len = (uint8_t)out_len;
    }
    hydro_hash_init_params(state);
    if (key != NULL) {
        uint8_t block[hydro_hash_BLOCKBYTES];
        memset(block, 0, sizeof block);
        mem_cpy(block, key, key_len);
        hydro_hash_update(state, block, sizeof block);
        hydro_memzero(block, sizeof block);
    }
    return 0;
}

int hydro_hash_update(hydro_hash_state *state, const uint8_t *in, size_t in_len)
{
    size_t left;
    size_t ps;
    size_t i;

    while (in_len > 0) {
        left = hydro_hash_BLOCKBYTES - state->buf_off;
        if ((ps = in_len) > left) {
            ps = left;
        }
        for (i = 0; i < ps; i++) {
            state->buf[state->buf_off + i] = in[i];
        }
        state->buf_off += ps;
        if (state->buf_off == hydro_hash_BLOCKBYTES) {
            hydro_hash_increment_counter(state, hydro_hash_BLOCKBYTES);
            hydro_hash_hashblock(state, state->buf);
            state->buf_off = 0;
        }
        in += ps;
        in_len -= ps;
    }
    return 0;
}

int hydro_hash_final(hydro_hash_state *state, uint8_t *out, size_t out_len)
{
    uint8_t  root[hydro_hash_BLOCKBYTES];
    uint32_t i;
    uint16_t xof_len;

    if (out_len < hydro_hash_BYTES_MIN || out_len > hydro_hash_BYTES_MAX) {
        return -1;
    }
    xof_len = LOAD16_LE(state->xof_len);
    if (xof_len == 0) {
        if (state->digest_len != out_len) {
            return -1;
        }
        return hydro_hash_blake2s_final(state, out, out_len);
    } else if (xof_len != out_len) {
        return -1;
    }
    if (hydro_hash_blake2s_final(state, root, hydro_hash_BLAKE2S_BYTES) != 0) {
        return -1;
    }
    state->key_len = 0;
    state->fanout  = 0;
    state->depth   = 0;
    STORE32_LE(state->leaf_len, hydro_hash_BLAKE2S_BYTES);
    state->inner_len = hydro_hash_BLAKE2S_BYTES;
    for (i = 0; out_len > 0; i++) {
        const size_t block_size = (out_len < hydro_hash_BLAKE2S_BYTES)
                                      ? out_len
                                      : hydro_hash_BLAKE2S_BYTES;
        state->digest_len = block_size;
        STORE32_LE(state->node_offset, i);
        hydro_hash_init_params(state);
        hydro_hash_update(state, root, hydro_hash_BLAKE2S_BYTES);
        if (hydro_hash_blake2s_final(
                state, out + i * hydro_hash_BLAKE2S_BYTES, block_size) != 0) {
            return -1;
        }
        out_len -= block_size;
    }
    return 0;
}

int hydro_hash_hash(uint8_t *out, size_t out_len, const uint8_t *in,
    size_t in_len, const uint8_t *key, size_t key_len)
{
    hydro_hash_state st;

    if (hydro_hash_init(&st, key, key_len, out_len) != 0 ||
        hydro_hash_update(&st, in, in_len) != 0 ||
        hydro_hash_final(&st, out, out_len) != 0) {
        return -1;
    }
    return 0;
}

void hydro_hash_keygen(uint8_t *key, size_t key_len)
{
    randombytes_buf(key, key_len);
}