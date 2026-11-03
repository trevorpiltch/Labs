/*
 * Test script for CRC-8.
 *
 * Build and run:
 *   gcc -std=c11 -Wall -Wextra -g -O0 -fsanitize=address,undefined test_crc8.c
 * -o test_crc8 && ./test_crc8
 *
 * Problem:
 *   Implement CRC-8 with these parameters (the common "CRC-8" / SMBus variant):
 *     polynomial  0x07   (x^8 + x^2 + x + 1)
 *     initial     0x00
 *     no input or output reflection, no final XOR
 *
 *   uint8_t crc8(const uint8_t *data, size_t len);
 *       CRC of a whole buffer. len == 0 returns the initial value (0x00).
 *
 *   uint8_t crc8_update(uint8_t crc, const uint8_t *data, size_t len);
 *       Continue from a running CRC, so a message can arrive in pieces:
 *       crc8(d, n) == crc8_update(0x00, d, n), and splitting the data anywhere
 * gives the same result.
 *
 * Known check value: crc8("123456789") == 0xF4.
 *
 * What the tests do:
 *   - fixed vectors (empty, single bytes, all zeros, all ones, "123456789",
 * ...)
 *   - the append property: crc8(message || crc8(message)) == 0
 *   - incremental vs. one-shot, split at every position and in random chunks
 *   - detection properties: every single-bit flip and every burst of <= 8 bits
 * changes the CRC
 *   - 20,000 random messages compared against a plain reference implementation
 * below
 *   - all input is allocated at exact size, so ASan flags any over-read
 *
 * Replace the stub section with your implementation (or #include your own file
 * instead).
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* TODO: implement these two (or #include your own file instead).      */
/* ------------------------------------------------------------------ */

uint8_t crc8_update(uint8_t crc, const uint8_t *data, size_t len) {
  size_t i = 0;

  while (i < len) {
    crc ^= data[i];

    for (int i = 0; i < 8; i++) {
      if ((crc >> 7) & 1U) {
        crc <<= 1;
        crc ^= 0x107;
      } else {
        crc <<= 1;
      }
    }

    i++;
  }

  return crc; /* TODO */
}

uint8_t crc8(const uint8_t *data, size_t len) {
  return crc8_update(0, data, len);
}

/* ------------------------------------------------------------------ */
/* Test harness                                                        */
/* ------------------------------------------------------------------ */

#define MAX_FAIL_LINES 40

static char ctx[160];
static int fail_lines = 0;

#define CHK(cond, ...)                                                         \
  do {                                                                         \
    if (!(cond)) {                                                             \
      ok = 0;                                                                  \
      if (fail_lines < MAX_FAIL_LINES) {                                       \
        printf("  FAIL [%s]: ", ctx);                                          \
        printf(__VA_ARGS__);                                                   \
        printf("\n");                                                          \
      }                                                                        \
      fail_lines++;                                                            \
    }                                                                          \
  } while (0)

static int tests_run = 0, tests_failed = 0;

static void tally(int ok) {
  tests_run++;
  if (!ok)
    tests_failed++;
}

static void *xmalloc(size_t n) {
  void *p = malloc(n ? n : 1);
  if (!p) {
    perror("malloc");
    exit(2);
  }
  return p;
}

/* Reference: the plain bit-by-bit definition. */
static uint8_t ref_crc8(const uint8_t *d, size_t n) {
  uint8_t crc = 0;
  for (size_t i = 0; i < n; i++) {
    crc ^= d[i];
    for (int b = 0; b < 8; b++)
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
  }
  return crc;
}

/* Copy bytes into an exact-size heap buffer so ASan catches over-reads. */
static uint8_t *dup_exact(const uint8_t *src, size_t n) {
  uint8_t *p = xmalloc(n);
  if (n)
    memcpy(p, src, n);
  return p;
}

/* ----------------------------- fixed vectors ----------------------- */

typedef struct {
  const char *name;
  uint8_t data[16];
  size_t len;
  uint8_t expected;
} vec_t;

static const vec_t vectors[] = {
    {"empty message", {0}, 0, 0x00},
    {"single 0x00", {0x00}, 1, 0x00},
    {"single 0x01", {0x01}, 1, 0x07},
    {"single 0x80", {0x80}, 1, 0x89},
    {"single 0xFF", {0xFF}, 1, 0xF3},
    {"single 0xAA", {0xAA}, 1, 0x5F},
    {"four zero bytes", {0, 0, 0, 0}, 4, 0x00},
    {"four 0xFF bytes", {0xFF, 0xFF, 0xFF, 0xFF}, 4, 0xDE},
    {"01 02 03", {0x01, 0x02, 0x03}, 3, 0x48},
    {"AA 03 01 02 03", {0xAA, 0x03, 0x01, 0x02, 0x03}, 5, 0x5C},
    {"\"hello\"", {'h', 'e', 'l', 'l', 'o'}, 5, 0x92},
    {"\"123456789\" (check value)",
     {'1', '2', '3', '4', '5', '6', '7', '8', '9'},
     9,
     0xF4},
};

static void run_vectors(void) {
  for (size_t i = 0; i < sizeof vectors / sizeof vectors[0]; i++) {
    const vec_t *v = &vectors[i];
    int ok = 1;
    snprintf(ctx, sizeof ctx, "%s", v->name);
    uint8_t *buf = dup_exact(v->data, v->len);

    uint8_t got = crc8(buf, v->len);
    CHK(got == v->expected, "crc8 = 0x%02X, expected 0x%02X", got, v->expected);

    uint8_t got2 = crc8_update(0x00, buf, v->len);
    CHK(got2 == v->expected, "crc8_update(0, ...) = 0x%02X, expected 0x%02X",
        got2, v->expected);

    free(buf);
    printf("[%s] %s -> 0x%02X\n", ok ? "PASS" : "FAIL", v->name, v->expected);
    tally(ok);
  }
}

/* ----------------------------- properties -------------------------- */

static void fill_random(uint8_t *p, size_t n) {
  for (size_t i = 0; i < n; i++)
    p[i] = (uint8_t)(rand() & 0xFF);
}

/* crc8(message || crc8(message)) == 0 */
static int test_append_property(size_t n) {
  int ok = 1;
  snprintf(ctx, sizeof ctx, "append property, len=%zu", n);
  uint8_t *msg = xmalloc(n + 1);
  fill_random(msg, n);
  msg[n] = crc8(msg, n);
  uint8_t got = crc8(msg, n + 1);
  CHK(got == 0, "crc of message plus its own CRC is 0x%02X, expected 0x00",
      got);
  free(msg);
  return ok;
}

/* Splitting a message at EVERY position must give the same CRC. */
static int test_split_everywhere(size_t n) {
  int ok = 1;
  uint8_t *msg = xmalloc(n);
  fill_random(msg, n);
  uint8_t want = ref_crc8(msg, n);

  for (size_t cut = 0; cut <= n; cut++) {
    snprintf(ctx, sizeof ctx, "split len=%zu at %zu", n, cut);
    uint8_t *a = dup_exact(msg, cut);
    uint8_t *b = dup_exact(msg + cut, n - cut);
    uint8_t c = crc8_update(0x00, a, cut);
    c = crc8_update(c, b, n - cut);
    CHK(c == want, "got 0x%02X, expected 0x%02X", c, want);
    free(a);
    free(b);
  }
  free(msg);
  return ok;
}

/* Feeding one byte at a time must also match. */
static int test_byte_at_a_time(size_t n) {
  int ok = 1;
  snprintf(ctx, sizeof ctx, "byte at a time, len=%zu", n);
  uint8_t *msg = xmalloc(n);
  fill_random(msg, n);

  uint8_t c = 0;
  for (size_t i = 0; i < n; i++) {
    uint8_t one = msg[i];
    c = crc8_update(c, &one, 1);
  }
  uint8_t want = ref_crc8(msg, n);
  CHK(c == want, "got 0x%02X, expected 0x%02X", c, want);
  free(msg);
  return ok;
}

/* Every single-bit flip must change the CRC. */
static int test_single_bit_flips(size_t n) {
  int ok = 1;
  snprintf(ctx, sizeof ctx, "single-bit flips, len=%zu", n);
  uint8_t *msg = xmalloc(n);
  fill_random(msg, n);
  uint8_t base = crc8(msg, n);

  for (size_t i = 0; i < n; i++)
    for (int b = 0; b < 8; b++) {
      msg[i] ^= (uint8_t)(1u << b);
      uint8_t c = crc8(msg, n);
      CHK(c != base, "flipping bit %d of byte %zu did not change the CRC", b,
          i);
      msg[i] ^= (uint8_t)(1u << b);
    }
  free(msg);
  return ok;
}

/* Every burst error of up to 8 bits (a nonzero pattern confined to an 8-bit
 * window) is detected. */
static int test_burst_errors(void) {
  int ok = 1;
  snprintf(ctx, sizeof ctx, "burst errors <= 8 bits");
  const size_t n = 12;
  uint8_t *msg = xmalloc(n);
  fill_random(msg, n);
  uint8_t base = crc8(msg, n);

  for (size_t start = 0; start + 8 <= n * 8;
       start++)                                /* start bit of the window */
    for (unsigned pat = 1; pat < 256; pat++) { /* nonzero 8-bit pattern    */
      uint8_t *m2 = dup_exact(msg, n);
      for (int b = 0; b < 8; b++)
        if (pat & (1u << b)) {
          size_t bit = start + (size_t)b;
          m2[bit / 8] ^= (uint8_t)(0x80u >> (bit % 8));
        }
      uint8_t c = crc8(m2, n);
      CHK(c != base, "burst pattern 0x%02X at bit %zu was not detected", pat,
          start);
      free(m2);
      if (!ok) {
        free(msg);
        return 0;
      }
    }
  free(msg);
  return ok;
}

/* ------------------------------ random ----------------------------- */

static int test_random_vs_reference(void) {
  int ok = 1;
  size_t n = (size_t)(rand() % 200);
  uint8_t tmp[200];
  fill_random(tmp, n);
  uint8_t *msg = dup_exact(tmp, n);

  snprintf(ctx, sizeof ctx, "random message, len=%zu", n);
  uint8_t want = ref_crc8(msg, n);
  uint8_t got = crc8(msg, n);
  CHK(got == want, "got 0x%02X, expected 0x%02X", got, want);

  /* also in random-sized chunks */
  uint8_t c = 0;
  size_t pos = 0;
  while (pos < n) {
    size_t chunk = 1 + (size_t)(rand() % 17);
    if (chunk > n - pos)
      chunk = n - pos;
    uint8_t *piece = dup_exact(msg + pos, chunk);
    c = crc8_update(c, piece, chunk);
    free(piece);
    pos += chunk;
  }
  CHK(c == want, "chunked crc8_update gave 0x%02X, expected 0x%02X", c, want);

  free(msg);
  return ok;
}

/* ------------------------------ main ------------------------------- */

int main(void) {
  int before;

  printf("--- fixed vectors ---\n");
  run_vectors();

  printf("\n--- properties ---\n");
  static const size_t sizes[] = {0, 1, 2, 3, 7, 8, 9, 16, 33, 64, 100};
  const size_t n_sizes = sizeof sizes / sizeof sizes[0];

  before = tests_failed;
  for (size_t i = 0; i < n_sizes; i++)
    tally(test_append_property(sizes[i]));
  printf("[%s] crc8(message || crc8(message)) == 0\n",
         tests_failed == before ? "PASS" : "FAIL");

  before = tests_failed;
  for (size_t i = 0; i < n_sizes; i++)
    tally(test_split_everywhere(sizes[i]));
  printf("[%s] crc8_update split at every position matches one-shot\n",
         tests_failed == before ? "PASS" : "FAIL");

  before = tests_failed;
  for (size_t i = 0; i < n_sizes; i++)
    tally(test_byte_at_a_time(sizes[i]));
  printf("[%s] feeding one byte at a time matches one-shot\n",
         tests_failed == before ? "PASS" : "FAIL");

  before = tests_failed;
  for (size_t i = 1; i < n_sizes; i++)
    tally(test_single_bit_flips(sizes[i]));
  printf("[%s] every single-bit flip changes the CRC\n",
         tests_failed == before ? "PASS" : "FAIL");

  before = tests_failed;
  tally(test_burst_errors());
  printf("[%s] every burst error of <= 8 bits changes the CRC\n",
         tests_failed == before ? "PASS" : "FAIL");

  printf("\n--- random messages vs. reference ---\n");
  before = tests_failed;
  srand(8888);
  const int ITER = 20000;
  for (int i = 0; i < ITER; i++)
    tally(test_random_vs_reference());
  printf("[%s] %d random messages (whole and chunked)\n",
         tests_failed == before ? "PASS" : "FAIL", ITER);

  if (fail_lines > MAX_FAIL_LINES)
    printf("\n(%d further failure lines suppressed)\n",
           fail_lines - MAX_FAIL_LINES);

  printf("\n%d/%d cases passed\n", tests_run - tests_failed, tests_run);
  return tests_failed != 0;
}
