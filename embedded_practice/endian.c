/*
 * Test script for endianness helpers and register bit fields.
 *
 * Build and run:
 *   gcc -std=c11 -Wall -Wextra -g -O0 -fsanitize=address,undefined test_endian_bits.c -o test_endian_bits && ./test_endian_bits
 *
 * Problem, part 1: endianness
 *   int      is_little_endian(void);        1 if this machine is little-endian, else 0. Detect it at run time.
 *   uint16_t swap16(uint16_t v);            reverse the 2 bytes of v.
 *   uint32_t swap32(uint32_t v);            reverse the 4 bytes of v, using shifts and masks.
 *
 *   Read and write multi-byte values in a byte buffer (a packet, say) with a stated byte order.
 *   The buffer may start at ANY address (not aligned), so do not cast `p` to a uint16_t* or uint32_t*.
 *     uint16_t read_be16(const uint8_t *p);   uint32_t read_be32(const uint8_t *p);
 *     uint16_t read_le16(const uint8_t *p);   uint32_t read_le32(const uint8_t *p);
 *     void write_be16(uint8_t *p, uint16_t v);   void write_be32(uint8_t *p, uint32_t v);
 *     void write_le16(uint8_t *p, uint16_t v);   void write_le32(uint8_t *p, uint32_t v);
 *   Each reads or writes exactly 2 or 4 bytes starting at p, and nothing else.
 *
 * Problem, part 2: register fields (explicit shifts and masks, NOT C bitfields)
 *   uint32_t field_get(uint32_t reg, unsigned pos, unsigned width);
 *       the `width` bits of reg starting at bit `pos` (bit 0 is the least significant), shifted down to bit 0.
 *   uint32_t field_set(uint32_t reg, unsigned pos, unsigned width, uint32_t value);
 *       reg with that field replaced by `value`. If value is wider than the field, only its low
 *       `width` bits are used: a too-wide value must never spill into neighboring bits.
 *   Valid fields have 1 <= width <= 32 and pos + width <= 32 (so width 32 at pos 0 is valid).
 *   For an invalid field, field_get returns 0 and field_set returns reg unchanged.
 *
 *   A status register, laid out like this:
 *       bit  0       ENABLE
 *       bit  1       IRQ_PENDING
 *       bits 3:2     SPEED     (2 bits)
 *       bits 7:4     GAIN      (4 bits)
 *       bits 15:8    COUNT     (8 bits)
 *       bits 31:16   ADDR      (16 bits)
 *   uint32_t status_pack(const status_t *s);        build the register value. Each field is masked to its
 *                                                   width, so an oversized field value cannot disturb its neighbors.
 *   void     status_unpack(uint32_t reg, status_t *s);   split a register value into its fields.
 *
 * What the tests do:
 *   - fixed vectors and random values for swaps, reads, and writes (reads and writes at 4 different
 *     alignments, with exact-size buffers so ASan catches over-reads, and canary bytes to catch stray writes)
 *   - UBSan (-fsanitize=undefined) catches the classic traps: misaligned pointer casts, signed shifts
 *     into the sign bit (for example (p[0] << 24) with p[0] >= 0x80), and shifts by 32
 *   - field_get and field_set for EVERY valid (pos, width) pair against a 64-bit reference, plus invalid fields
 *   - status_pack and status_unpack: each field alone, all fields maximal, oversized values, and round trips
 *
 * Replace the stub section with your implementation (or #include your own file instead).
 */

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t  enable;
    uint8_t  irq_pending;
    uint8_t  speed;
    uint8_t  gain;
    uint8_t  count;
    uint16_t addr;
} status_t;

/* ------------------------------------------------------------------ */
/* TODO: implement these (or #include your own file instead).          */
/* ------------------------------------------------------------------ */

int is_little_endian(void) {
  uint16_t test = 0x0102;
  uint16_t* test_ptr = &test;
  uint8_t lowest;
  uint8_t* lowest_ptr = &lowest;
  uint8_t highest;
  memcpy(lowest_ptr, test_ptr, 1);

  return lowest == 0x02;
}

uint16_t swap16(uint16_t v) { 
  uint8_t tp8 = v >> 8;
  v <<= 8;
  v |= tp8;
  return v;
}

uint32_t swap32(uint32_t v) { 
  uint32_t res = 0;
  for (int i=0; i<4; i++) {
    res = (res << 8);
    res |= (v & 0xFFu);
    v >>= 8;
  }
  return res;
}

uint16_t read_be16(const uint8_t *p) { (void)p; return 0; /* TODO */ }
uint32_t read_be32(const uint8_t *p) { (void)p; return 0; /* TODO */ }
uint16_t read_le16(const uint8_t *p) { (void)p; return 0; /* TODO */ }
uint32_t read_le32(const uint8_t *p) { (void)p; return 0; /* TODO */ }

void write_be16(uint8_t *p, uint16_t v) { (void)p; (void)v; /* TODO */ }
void write_be32(uint8_t *p, uint32_t v) { (void)p; (void)v; /* TODO */ }
void write_le16(uint8_t *p, uint16_t v) { (void)p; (void)v; /* TODO */ }
void write_le32(uint8_t *p, uint32_t v) { (void)p; (void)v; /* TODO */ }

uint32_t field_get(uint32_t reg, unsigned pos, unsigned width) {
    (void)reg; (void)pos; (void)width;
    return 0;          /* TODO */
}

uint32_t field_set(uint32_t reg, unsigned pos, unsigned width, uint32_t value) {
    (void)pos; (void)width; (void)value;
    return reg;        /* TODO */
}

uint32_t status_pack(const status_t *s) { (void)s; return 0; /* TODO */ }
void status_unpack(uint32_t reg, status_t *s) { (void)reg; (void)s; /* TODO */ }

/* ------------------------------------------------------------------ */
/* Test harness                                                        */
/* ------------------------------------------------------------------ */

#define MAX_FAIL_LINES 40
#define CANARY 0xEE
#define TAIL   8

static char ctx[160];
static int  fail_lines = 0;

#define CHK(cond, ...)                                           \
    do {                                                         \
        if (!(cond)) {                                           \
            ok = 0;                                              \
            if (fail_lines < MAX_FAIL_LINES) {                   \
                printf("  FAIL [%s]: ", ctx);                    \
                printf(__VA_ARGS__);                             \
                printf("\n");                                    \
            }                                                    \
            fail_lines++;                                        \
        }                                                        \
    } while (0)

static int tests_run = 0, tests_failed = 0;

static void tally(int ok) {
    tests_run++;
    if (!ok) tests_failed++;
}

static void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) { perror("malloc"); exit(2); }
    return p;
}

static uint32_t rnd32(void) {
    return ((uint32_t)(rand() & 0xFFFF) << 16) ^ (uint32_t)(rand() & 0xFFFF) ^ ((uint32_t)(rand() & 1) << 15);
}

static int all_equal(const uint8_t *p, size_t n, uint8_t v) {
    for (size_t i = 0; i < n; i++)
        if (p[i] != v) return 0;
    return 1;
}

/* ----------------------------- references -------------------------- */

static uint32_t ref_swap32(uint32_t v) {
    uint32_t r = 0;
    for (int i = 0; i < 4; i++) { r = (r << 8) | (v & 0xFFu); v >>= 8; }
    return r;
}

static uint16_t ref_swap16(uint16_t v) { return (uint16_t)(((v & 0xFFu) << 8) | (v >> 8)); }

static uint32_t ref_be(const uint8_t *p, int n) {
    uint32_t r = 0;
    for (int i = 0; i < n; i++) r = (r << 8) | p[i];
    return r;
}

static uint32_t ref_le(const uint8_t *p, int n) {
    uint32_t r = 0;
    for (int i = n - 1; i >= 0; i--) r = (r << 8) | p[i];
    return r;
}

static int field_valid(unsigned pos, unsigned width) {
    return width >= 1 && width <= 32 && pos < 32 && pos + width <= 32;
}

static uint32_t ref_get(uint32_t reg, unsigned pos, unsigned width) {
    if (!field_valid(pos, width)) return 0;
    uint64_t mask = (1ULL << width) - 1ULL;
    return (uint32_t)(((uint64_t)reg >> pos) & mask);
}

static uint32_t ref_set(uint32_t reg, unsigned pos, unsigned width, uint32_t value) {
    if (!field_valid(pos, width)) return reg;
    uint64_t mask = (1ULL << width) - 1ULL;
    uint64_t r = ((uint64_t)reg & ~(mask << pos)) | (((uint64_t)value & mask) << pos);
    return (uint32_t)r;
}

static uint32_t ref_pack(const status_t *s) {
    uint32_t r = 0;
    r = ref_set(r, 0, 1, s->enable);
    r = ref_set(r, 1, 1, s->irq_pending);
    r = ref_set(r, 2, 2, s->speed);
    r = ref_set(r, 4, 4, s->gain);
    r = ref_set(r, 8, 8, s->count);
    r = ref_set(r, 16, 16, s->addr);
    return r;
}

/* ----------------------------- endianness -------------------------- */

static int test_is_little_endian(void) {
    int ok = 1;
    snprintf(ctx, sizeof ctx, "is_little_endian");
    uint16_t probe = 1;
    uint8_t first;
    memcpy(&first, &probe, 1);
    int want = (first == 1);
    int got = is_little_endian();
    CHK(got == want, "returned %d, this machine is %s-endian (expected %d)", got, want ? "little" : "big", want);
    return ok;
}

static const struct { uint32_t v; uint32_t s; } swap32_vectors[] = {
    { 0x11223344u, 0x44332211u },
    { 0x00000000u, 0x00000000u },
    { 0xFFFFFFFFu, 0xFFFFFFFFu },
    { 0x000000FFu, 0xFF000000u },
    { 0xFF000000u, 0x000000FFu },
    { 0x80000000u, 0x00000080u },
    { 0x00000080u, 0x80000000u },
    { 0x12345678u, 0x78563412u },
    { 0xDEADBEEFu, 0xEFBEADDEu },
};

static const struct { uint16_t v; uint16_t s; } swap16_vectors[] = {
    { 0x1122u, 0x2211u },
    { 0x0000u, 0x0000u },
    { 0xFFFFu, 0xFFFFu },
    { 0x00FFu, 0xFF00u },
    { 0xFF00u, 0x00FFu },
    { 0x8000u, 0x0080u },
    { 0x0080u, 0x8000u },
    { 0xABCDu, 0xCDABu },
};

static void run_swap_tests(void) {
    for (size_t i = 0; i < sizeof swap32_vectors / sizeof swap32_vectors[0]; i++) {
        int ok = 1;
        snprintf(ctx, sizeof ctx, "swap32(0x%08X)", swap32_vectors[i].v);
        uint32_t got = swap32(swap32_vectors[i].v);
        CHK(got == swap32_vectors[i].s, "got 0x%08X, expected 0x%08X", got, swap32_vectors[i].s);
        printf("[%s] swap32(0x%08X) = 0x%08X\n", ok ? "PASS" : "FAIL", swap32_vectors[i].v, swap32_vectors[i].s);
        tally(ok);
    }
    for (size_t i = 0; i < sizeof swap16_vectors / sizeof swap16_vectors[0]; i++) {
        int ok = 1;
        snprintf(ctx, sizeof ctx, "swap16(0x%04X)", swap16_vectors[i].v);
        uint16_t got = swap16(swap16_vectors[i].v);
        CHK(got == swap16_vectors[i].s, "got 0x%04X, expected 0x%04X", got, swap16_vectors[i].s);
        printf("[%s] swap16(0x%04X) = 0x%04X\n", ok ? "PASS" : "FAIL", swap16_vectors[i].v, swap16_vectors[i].s);
        tally(ok);
    }

    int before = tests_failed;
    for (int i = 0; i < 20000; i++) {
        int ok = 1;
        uint32_t v = rnd32();
        snprintf(ctx, sizeof ctx, "swap32 random 0x%08X", v);
        CHK(swap32(v) == ref_swap32(v), "got 0x%08X, expected 0x%08X", swap32(v), ref_swap32(v));
        CHK(swap32(swap32(v)) == v, "swapping twice did not give back 0x%08X", v);
        uint16_t w = (uint16_t)(v >> 7);
        snprintf(ctx, sizeof ctx, "swap16 random 0x%04X", w);
        CHK(swap16(w) == ref_swap16(w), "got 0x%04X, expected 0x%04X", swap16(w), ref_swap16(w));
        CHK(swap16(swap16(w)) == w, "swapping twice did not give back 0x%04X", w);
        tally(ok);
    }
    printf("[%s] swap16 and swap32: 20000 random values vs. reference, and swapping twice is the identity\n",
           tests_failed == before ? "PASS" : "FAIL");
}

/* Reads at 4 different alignments with exact-size buffers (ASan catches over-reads, UBSan catches misaligned casts). */
static int test_reads(const uint8_t b[4], size_t off) {
    int ok = 1;
    snprintf(ctx, sizeof ctx, "reads of %02X %02X %02X %02X at offset %zu", b[0], b[1], b[2], b[3], off);

    uint8_t *buf2 = xmalloc(off + 2);
    memset(buf2, CANARY, off);
    memcpy(buf2 + off, b, 2);
    uint8_t *buf4 = xmalloc(off + 4);
    memset(buf4, CANARY, off);
    memcpy(buf4 + off, b, 4);

    uint16_t be16 = read_be16(buf2 + off);
    uint16_t le16 = read_le16(buf2 + off);
    uint32_t be32 = read_be32(buf4 + off);
    uint32_t le32 = read_le32(buf4 + off);

    CHK(be16 == ref_be(b, 2), "read_be16 = 0x%04X, expected 0x%04X", be16, ref_be(b, 2));
    CHK(le16 == ref_le(b, 2), "read_le16 = 0x%04X, expected 0x%04X", le16, ref_le(b, 2));
    CHK(be32 == ref_be(b, 4), "read_be32 = 0x%08X, expected 0x%08X", be32, ref_be(b, 4));
    CHK(le32 == ref_le(b, 4), "read_le32 = 0x%08X, expected 0x%08X", le32, ref_le(b, 4));

    free(buf2); free(buf4);
    return ok;
}

/* Writes at 4 different alignments: right bytes, nothing before or after. */
static int test_writes(uint32_t v, size_t off) {
    int ok = 1;
    snprintf(ctx, sizeof ctx, "writes of 0x%08X at offset %zu", v, off);

    uint8_t want[4];

    /* 32-bit */
    for (int be = 0; be < 2; be++) {
        uint8_t *buf = xmalloc(off + 4 + TAIL);
        memset(buf, CANARY, off + 4 + TAIL);
        for (int i = 0; i < 4; i++)
            want[i] = be ? (uint8_t)(v >> (24 - 8 * i)) : (uint8_t)(v >> (8 * i));
        if (be) write_be32(buf + off, v); else write_le32(buf + off, v);
        CHK(memcmp(buf + off, want, 4) == 0, "write_%s32 bytes: %02X %02X %02X %02X, expected %02X %02X %02X %02X",
            be ? "be" : "le", buf[off], buf[off + 1], buf[off + 2], buf[off + 3], want[0], want[1], want[2], want[3]);
        CHK(all_equal(buf, off, CANARY), "write_%s32 wrote BEFORE p", be ? "be" : "le");
        CHK(all_equal(buf + off + 4, TAIL, CANARY), "write_%s32 wrote more than 4 bytes", be ? "be" : "le");
        free(buf);
    }

    /* 16-bit */
    uint16_t w = (uint16_t)v;
    for (int be = 0; be < 2; be++) {
        uint8_t *buf = xmalloc(off + 2 + TAIL);
        memset(buf, CANARY, off + 2 + TAIL);
        want[0] = be ? (uint8_t)(w >> 8) : (uint8_t)w;
        want[1] = be ? (uint8_t)w : (uint8_t)(w >> 8);
        if (be) write_be16(buf + off, w); else write_le16(buf + off, w);
        CHK(memcmp(buf + off, want, 2) == 0, "write_%s16 bytes: %02X %02X, expected %02X %02X",
            be ? "be" : "le", buf[off], buf[off + 1], want[0], want[1]);
        CHK(all_equal(buf, off, CANARY), "write_%s16 wrote BEFORE p", be ? "be" : "le");
        CHK(all_equal(buf + off + 2, TAIL, CANARY), "write_%s16 wrote more than 2 bytes", be ? "be" : "le");
        free(buf);
    }
    return ok;
}

static int test_roundtrip(uint32_t v, size_t off) {
    int ok = 1;
    snprintf(ctx, sizeof ctx, "write then read of 0x%08X at offset %zu", v, off);
    uint8_t *buf = xmalloc(off + 4);
    write_be32(buf + off, v);
    CHK(read_be32(buf + off) == v, "be32 round trip gave 0x%08X", read_be32(buf + off));
    write_le32(buf + off, v);
    CHK(read_le32(buf + off) == v, "le32 round trip gave 0x%08X", read_le32(buf + off));
    write_be16(buf + off, (uint16_t)v);
    CHK(read_be16(buf + off) == (uint16_t)v, "be16 round trip gave 0x%04X", read_be16(buf + off));
    write_le16(buf + off, (uint16_t)v);
    CHK(read_le16(buf + off) == (uint16_t)v, "le16 round trip gave 0x%04X", read_le16(buf + off));
    /* the same four bytes read in the other order must be the byte swap */
    write_be32(buf + off, v);
    CHK(read_le32(buf + off) == ref_swap32(v), "reading big-endian bytes as little-endian is not the byte swap");
    free(buf);
    return ok;
}

static void run_read_write_tests(void) {
    static const uint8_t patterns[][4] = {
        {0x12,0x34,0x56,0x78}, {0xFF,0x80,0x01,0x00}, {0x00,0x00,0x00,0x00}, {0xFF,0xFF,0xFF,0xFF},
        {0x80,0x00,0x00,0x01}, {0x01,0x02,0x03,0x04}, {0xA5,0x5A,0xC3,0x3C}, {0x00,0x00,0x00,0x80},
    };
    int before = tests_failed, count = 0;
    for (size_t i = 0; i < sizeof patterns / sizeof patterns[0]; i++)
        for (size_t off = 0; off < 4; off++) { tally(test_reads(patterns[i], off)); count++; }
    printf("[%s] read_be16/le16/be32/le32 at 4 alignments, including bytes >= 0x80 (%d cases)\n",
           tests_failed == before ? "PASS" : "FAIL", count);

    before = tests_failed; count = 0;
    static const uint32_t vals[] = { 0x12345678u, 0xFF800100u, 0u, 0xFFFFFFFFu, 0x80000001u, 0x000000FFu, 0xDEADBEEFu };
    for (size_t i = 0; i < sizeof vals / sizeof vals[0]; i++)
        for (size_t off = 0; off < 4; off++) { tally(test_writes(vals[i], off)); count++; }
    printf("[%s] write_be16/le16/be32/le32 at 4 alignments, nothing written outside the field (%d cases)\n",
           tests_failed == before ? "PASS" : "FAIL", count);

    before = tests_failed;
    srand(2468);
    for (int i = 0; i < 10000; i++) {
        uint32_t v = rnd32();
        tally(test_roundtrip(v, (size_t)(rand() % 4)));
        uint8_t b[4] = { (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v };
        tally(test_reads(b, (size_t)(rand() % 4)));
        tally(test_writes(v, (size_t)(rand() % 4)));
    }
    printf("[%s] 10000 random values: reads, writes, and round trips\n", tests_failed == before ? "PASS" : "FAIL");
}

/* ----------------------------- fields ------------------------------ */

static int test_field_pair(unsigned pos, unsigned width) {
    int ok = 1;
    for (int k = 0; k < 4; k++) {
        uint32_t reg = (k == 0) ? 0u : (k == 1) ? 0xFFFFFFFFu : rnd32();
        uint32_t val = (k == 0) ? 0xFFFFFFFFu : (k == 1) ? 0u : rnd32();
        snprintf(ctx, sizeof ctx, "pos=%u width=%u reg=0x%08X value=0x%08X", pos, width, reg, val);
        uint32_t g = field_get(reg, pos, width);
        CHK(g == ref_get(reg, pos, width), "field_get = 0x%08X, expected 0x%08X", g, ref_get(reg, pos, width));
        uint32_t s = field_set(reg, pos, width, val);
        CHK(s == ref_set(reg, pos, width, val), "field_set = 0x%08X, expected 0x%08X", s, ref_set(reg, pos, width, val));
        CHK(field_get(s, pos, width) == ref_get(reg == s ? reg : s, pos, width), "field_get after field_set is inconsistent");
    }
    return ok;
}

static void run_field_tests(void) {
    static const struct { const char *name; uint32_t reg; unsigned pos, width; uint32_t value; int is_set; uint32_t want; } vecs[] = {
        { "get: byte 1 of 0xDEADBEEF",                   0xDEADBEEFu, 8, 8, 0,           0, 0xBEu },
        { "get: the whole word (width 32)",              0xDEADBEEFu, 0, 32, 0,          0, 0xDEADBEEFu },
        { "get: the top bit",                            0x80000000u, 31, 1, 0,          0, 1u },
        { "get: top nibble",                             0xF0000000u, 28, 4, 0,          0, 0xFu },
        { "set: nibble at bit 4",                        0x00000000u, 4, 4, 0xA,         1, 0x000000A0u },
        { "set: clear a byte inside all ones",           0xFFFFFFFFu, 8, 8, 0,           1, 0xFFFF00FFu },
        { "set: whole word (width 32)",                  0x00000000u, 0, 32, 0x12345678u, 1, 0x12345678u },
        { "set: value too wide must not spill",          0x00000000u, 28, 4, 0x1Fu,      1, 0xF0000000u },
        { "set: one-bit field, value 2 uses only bit 0", 0xFFFFFFFFu, 0, 1, 2,           1, 0xFFFFFFFEu },
        { "set: top bit",                                0x00000000u, 31, 1, 1,          1, 0x80000000u },
    };
    for (size_t i = 0; i < sizeof vecs / sizeof vecs[0]; i++) {
        int ok = 1;
        snprintf(ctx, sizeof ctx, "%s", vecs[i].name);
        uint32_t got = vecs[i].is_set ? field_set(vecs[i].reg, vecs[i].pos, vecs[i].width, vecs[i].value)
                                      : field_get(vecs[i].reg, vecs[i].pos, vecs[i].width);
        CHK(got == vecs[i].want, "got 0x%08X, expected 0x%08X", got, vecs[i].want);
        printf("[%s] %s\n", ok ? "PASS" : "FAIL", vecs[i].name);
        tally(ok);
    }

    int before = tests_failed, count = 0;
    srand(1357);
    for (unsigned pos = 0; pos < 32; pos++)
        for (unsigned width = 1; width <= 32 - pos; width++) { tally(test_field_pair(pos, width)); count++; }
    printf("[%s] field_get and field_set for every valid (pos, width) pair vs. a 64-bit reference (%d pairs)\n",
           tests_failed == before ? "PASS" : "FAIL", count);

    before = tests_failed;
    {
        static const struct { unsigned pos, width; } bad[] = {
            { 0, 0 }, { 5, 0 }, { 31, 0 }, { 32, 1 }, { 31, 2 }, { 16, 17 }, { 0, 33 }, { 40, 1 }, { 1, 32 }, { 100, 100 },
        };
        for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            int ok = 1;
            snprintf(ctx, sizeof ctx, "invalid field pos=%u width=%u", bad[i].pos, bad[i].width);
            uint32_t reg = 0xA5A5A5A5u;
            CHK(field_get(reg, bad[i].pos, bad[i].width) == 0, "field_get should return 0 for an invalid field");
            CHK(field_set(reg, bad[i].pos, bad[i].width, 0xFFFFFFFFu) == reg, "field_set should return reg unchanged for an invalid field");
            tally(ok);
        }
    }
    printf("[%s] invalid fields (width 0, width > 32, pos + width > 32): get returns 0, set leaves reg unchanged\n",
           tests_failed == before ? "PASS" : "FAIL");
}

/* ----------------------------- status register --------------------- */

static int test_status_case(const char *name, const status_t *s) {
    int ok = 1;
    snprintf(ctx, sizeof ctx, "%s", name);
    uint32_t want = ref_pack(s);
    uint32_t got = status_pack(s);
    CHK(got == want, "status_pack = 0x%08X, expected 0x%08X", got, want);

    status_t u;
    memset(&u, 0xEE, sizeof u);
    status_unpack(got, &u);
    CHK(u.enable == (s->enable & 1u), "enable = %u, expected %u", u.enable, s->enable & 1u);
    CHK(u.irq_pending == (s->irq_pending & 1u), "irq_pending = %u, expected %u", u.irq_pending, s->irq_pending & 1u);
    CHK(u.speed == (s->speed & 3u), "speed = %u, expected %u", u.speed, s->speed & 3u);
    CHK(u.gain == (s->gain & 0xFu), "gain = %u, expected %u", u.gain, s->gain & 0xFu);
    CHK(u.count == s->count, "count = %u, expected %u", u.count, s->count);
    CHK(u.addr == s->addr, "addr = 0x%04X, expected 0x%04X", u.addr, s->addr);
    return ok;
}

static void run_status_tests(void) {
    static const struct { const char *name; status_t s; uint32_t reg; } cases[] = {
        { "all fields zero",            {0, 0, 0, 0, 0, 0},                 0x00000000u },
        { "ENABLE alone",               {1, 0, 0, 0, 0, 0},                 0x00000001u },
        { "IRQ_PENDING alone",          {0, 1, 0, 0, 0, 0},                 0x00000002u },
        { "SPEED alone (3)",            {0, 0, 3, 0, 0, 0},                 0x0000000Cu },
        { "GAIN alone (15)",            {0, 0, 0, 0xF, 0, 0},               0x000000F0u },
        { "COUNT alone (255)",          {0, 0, 0, 0, 0xFF, 0},              0x0000FF00u },
        { "ADDR alone (0xFFFF)",        {0, 0, 0, 0, 0, 0xFFFF},            0xFFFF0000u },
        { "all fields maximal",         {1, 1, 3, 0xF, 0xFF, 0xFFFF},       0xFFFFFFFFu },
        { "a mixed value",              {1, 0, 2, 5, 0x42, 0x1234},         0x12344251u },
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        int ok = test_status_case(cases[i].name, &cases[i].s);
        uint32_t got = status_pack(&cases[i].s);
        snprintf(ctx, sizeof ctx, "%s", cases[i].name);
        CHK(got == cases[i].reg, "status_pack = 0x%08X, expected 0x%08X", got, cases[i].reg);
        printf("[%s] status: %s -> 0x%08X\n", ok ? "PASS" : "FAIL", cases[i].name, cases[i].reg);
        tally(ok);
    }

    {
        int ok = 1;
        snprintf(ctx, sizeof ctx, "unpack of 0xFFFFFFFF");
        status_t u;
        memset(&u, 0, sizeof u);
        status_unpack(0xFFFFFFFFu, &u);
        CHK(u.enable == 1 && u.irq_pending == 1 && u.speed == 3 && u.gain == 15 && u.count == 255 && u.addr == 0xFFFF,
            "fields: enable=%u irq=%u speed=%u gain=%u count=%u addr=0x%04X", u.enable, u.irq_pending, u.speed, u.gain, u.count, u.addr);
        printf("[%s] status: unpack of 0xFFFFFFFF\n", ok ? "PASS" : "FAIL");
        tally(ok);
    }

    int before = tests_failed;
    {
        /* oversized field values must be masked and must not spill into neighbors */
        static const status_t big[] = {
            { 0xFF, 0, 0, 0, 0, 0 }, { 0, 0xFF, 0, 0, 0, 0 }, { 0, 0, 0xFF, 0, 0, 0 },
            { 0, 0, 0, 0xFF, 0, 0 }, { 2, 2, 4, 0x10, 0, 0 }, { 0xFF, 0xFF, 0xFF, 0xFF, 0, 0 },
        };
        for (size_t i = 0; i < sizeof big / sizeof big[0]; i++) {
            char nm[48];
            snprintf(nm, sizeof nm, "oversized field values, case %zu", i);
            tally(test_status_case(nm, &big[i]));
        }
    }
    printf("[%s] oversized field values are masked and never disturb neighboring fields (6 cases)\n",
           tests_failed == before ? "PASS" : "FAIL");

    before = tests_failed;
    srand(9753);
    for (int i = 0; i < 20000; i++) {
        status_t s = { (uint8_t)rand(), (uint8_t)rand(), (uint8_t)rand(), (uint8_t)rand(), (uint8_t)rand(), (uint16_t)rand() };
        tally(test_status_case("random status", &s));
        int ok = 1;
        uint32_t reg = rnd32();
        status_t u;
        status_unpack(reg, &u);
        snprintf(ctx, sizeof ctx, "pack(unpack(0x%08X))", reg);
        CHK(status_pack(&u) == reg, "pack(unpack(reg)) = 0x%08X, expected 0x%08X", status_pack(&u), reg);
        tally(ok);
    }
    printf("[%s] 20000 random status structs and register values: pack, unpack, and round trips\n",
           tests_failed == before ? "PASS" : "FAIL");
}

/* ------------------------------ main ------------------------------- */

int main(void) {
    printf("--- endianness ---\n");
    {
        int ok = test_is_little_endian();
        printf("[%s] is_little_endian\n", ok ? "PASS" : "FAIL");
        tally(ok);
    }
    run_swap_tests();
    run_read_write_tests();

    printf("\n--- register fields ---\n");
    run_field_tests();

    printf("\n--- status register ---\n");
    run_status_tests();

    if (fail_lines > MAX_FAIL_LINES)
        printf("\n(%d further failure lines suppressed)\n", fail_lines - MAX_FAIL_LINES);

    printf("\n%d/%d cases passed\n", tests_run - tests_failed, tests_run);
    return tests_failed != 0;
}
