/*
 * Test script for integer-to-string conversion.
 *
 * Build and run:
 *   gcc -std=c11 -Wall -Wextra -g -O0 -fsanitize=address,undefined test_itoa.c
 * -o test_itoa && ./test_itoa
 *
 * Problem:
 *   Convert integers to text without sprintf, snprintf, or any other library
 * formatting function, writing into a buffer supplied by the caller (no
 * malloc).
 *
 *     size_t int_to_str(int value, char *buf, size_t buf_size);
 *         Writes the decimal text of `value` followed by a terminating '\0'
 * into buf. Negative numbers start with '-'. No '+' sign and no leading zeros
 * (zero is "0"). Returns the length of the text, not counting the '\0'. If the
 * text and its '\0' do not fit in buf_size bytes, returns 0 and must not write
 * past buf[buf_size - 1]. (A successful result is never 0, because the shortest
 * text is "0".) This must work for every int, including INT_MIN.
 *
 *     size_t utoa_base(uint32_t value, unsigned base, char *buf, size_t
 * buf_size); The same, for an unsigned value in any base from 2 to 16, using
 * lowercase digits (0-9, a-f), no prefix such as "0x" and no sign. For a base
 * outside 2..16, returns 0.
 *
 * Examples:
 *     int_to_str(0, buf, 8)           -> 1, buf = "0"
 *     int_to_str(-45, buf, 8)         -> 3, buf = "-45"
 *     int_to_str(2147483647, buf, 11) -> 10, buf = "2147483647"
 *     int_to_str(INT_MIN, buf, 12)    -> 11, buf = "-2147483648"
 *     int_to_str(12345, buf, 5)       -> 0  (needs 6 bytes: five digits and the
 * '\0') utoa_base(255, 16, buf, 8)      -> 2, buf = "ff" utoa_base(5, 2, buf,
 * 8)         -> 3, buf = "101"
 *
 * What the tests do:
 *   - named values, including 0, INT_MIN, INT_MAX, and powers of ten with their
 * neighbors
 *   - for every value: a buffer of exactly the right size, then EVERY smaller
 * size from 0 up (must return 0 and write nothing past the end), then a larger
 * buffer (nothing written past the '\0')
 *   - every int from -2000 to 2000, then 100,000 random ints against
 * snprintf("%d")
 *   - utoa_base for every base from 2 to 16 against a reference, plus invalid
 * bases
 *   - canary bytes around every buffer, so a write one byte too far is detected
 * even without ASan
 *
 * Replace the stub section with your implementation (or #include your own file
 * instead).
 */

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* TODO: implement these (or #include your own file instead).          */
/* ------------------------------------------------------------------ */

size_t int_to_str(int value, char *buf, size_t buf_size) {
  if (buf_size < 2) {
    return 0;
  }

  int i = 0;
  int is_negative = value < 0;
  unsigned num;

  if (value == 0) {
    buf[0] = '0';
    buf[1] = '\0';
    return 1;
  }

  if (value < 0) {
    num = 0u - (unsigned int)value;
  }
  else {
    num = value;
  }

  while (num != 0 && i < buf_size) {
    int remainder = num % 10;
    buf[i] = remainder + '0';
    i++;
    num /= 10;
  }

  if (i < buf_size && is_negative) {
    buf[i] = '-';
    i++;
  }

  if (i < buf_size) {
    buf[i] = '\0';
  } else {
    return 0;
  }

  // reverse
  int start = 0;
  int end = i - 1;

  while (start < end) {
    char tmp = buf[start];
    buf[start] = buf[end];
    buf[end] = tmp;
    start++;
    end--;
  }

  return (size_t)i;
}

size_t utoa_base(uint32_t value, unsigned base, char *buf, size_t buf_size) {
  if (buf_size < 2) {
    return 0;
  }

  if (base < 2 || base > 16) {
    return 0;
  }

  int i = 0;

  if (value == 0) {
    buf[0] = '0';
    buf[1] = '\0';
    return 1;
  }

  while (value != 0 && i < buf_size) {
    int remainder = value % base;
    if (remainder > 9) {
      buf[i] = (remainder - 10) + 'a';
    }
    else {
      buf[i] = remainder + '0';
    }
    i++;
    value /= base;
  }

  if (i < buf_size) {
    buf[i] = '\0';
  } else {
    return 0;
  }

  // reverse
  int start = 0;
  int end = i - 1;

  while (start < end) {
    char tmp = buf[start];
    buf[start] = buf[end];
    buf[end] = tmp;
    start++;
    end--;
  }
  
  return (size_t)i;
}

/* ------------------------------------------------------------------ */
/* Test harness                                                        */
/* ------------------------------------------------------------------ */

#define MAX_FAIL_LINES 40
#define CANARY 0xEE
#define TAIL 8

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

static int all_canary(const unsigned char *p, size_t n) {
  for (size_t i = 0; i < n; i++)
    if (p[i] != CANARY)
      return 0;
  return 1;
}

/* Show n bytes of a buffer for a failure message: printable characters as they
 * are, anything else as \xNN. */
static const char *show(const void *p, size_t n) {
  static char out[160];
  const unsigned char *s = p;
  size_t k = 0;
  for (size_t i = 0; i < n && k + 5 < sizeof out; i++) {
    if (s[i] >= 32 && s[i] < 127)
      out[k++] = (char)s[i];
    else
      k += (size_t)snprintf(out + k, sizeof out - k, "\\x%02X", s[i]);
  }
  out[k] = '\0';
  return out;
}

/* Reference: the text of an unsigned value in a base, built from 64-bit
 * arithmetic. */
static size_t ref_utoa(uint32_t v, unsigned base, char *out) {
  static const char digits[] = "0123456789abcdef";
  char tmp[40];
  size_t n = 0;
  uint64_t x = v;
  do {
    tmp[n++] = digits[x % base];
    x /= base;
  } while (x);
  for (size_t i = 0; i < n; i++)
    out[i] = tmp[n - 1 - i];
  out[n] = '\0';
  return n;
}

/*
 * Check a conversion at every buffer size around the real one.
 * kind 0 = int_to_str(value), kind 1 = utoa_base(value, base).
 */
static int check_conversion(int kind, long long value, unsigned base,
                            const char *want) {
  int ok = 1;
  size_t len = strlen(want);

#define CALL(buf, cap)                                                         \
  (kind == 0 ? int_to_str((int)value, (buf), (cap))                            \
             : utoa_base((uint32_t)value, base, (buf), (cap)))

  /* 1. exactly enough room: the text plus the '\0' */
  {
    size_t cap = len + 1;
    unsigned char *b = xmalloc(cap + TAIL + 1);
    memset(b, CANARY, cap + TAIL + 1);
    size_t r = CALL((char *)b, cap);
    CHK(r == len, "exact buffer (%zu bytes): returned %zu, expected %zu", cap,
        r, len);
    CHK(memcmp(b, want, len + 1) == 0,
        "exact buffer: bytes are \"%s\", expected \"%s\" plus a '\\0'",
        show(b, len + 1), want);
    CHK(all_canary(b + cap, TAIL + 1),
        "exact buffer: wrote past the end of the buffer");
    free(b);
  }

  /* 2. every smaller size, 0 up to len: must return 0 and write nothing past
   * the end */
  for (size_t cap = 0; cap <= len; cap++) {
    unsigned char *b = xmalloc(cap + TAIL);
    memset(b, CANARY, cap + TAIL);
    size_t r = CALL((char *)b, cap);
    CHK(r == 0, "buffer of %zu bytes (needs %zu): returned %zu, expected 0",
        cap, len + 1, r);
    CHK(all_canary(b + cap, TAIL),
        "buffer of %zu bytes (needs %zu): wrote past the end", cap, len + 1);
    free(b);
  }

  /* 3. plenty of room: the text, the '\0', and nothing else */
  {
    size_t cap = len + 1 + 16;
    unsigned char *b = xmalloc(cap);
    memset(b, CANARY, cap);
    size_t r = CALL((char *)b, cap);
    CHK(r == len, "large buffer: returned %zu, expected %zu", r, len);
    CHK(memcmp(b, want, len + 1) == 0, "large buffer: text differs from \"%s\"",
        want);
    CHK(all_canary(b + len + 1, cap - len - 1),
        "large buffer: wrote more bytes than the text and its '\\0'");
    free(b);
  }
#undef CALL
  return ok;
}

static int check_int(int v) {
  char want[16];
  snprintf(want, sizeof want, "%d", v);
  snprintf(ctx, sizeof ctx, "int_to_str(%d)", v);
  return check_conversion(0, v, 10, want);
}

static int check_base(uint32_t v, unsigned base) {
  char want[40];
  ref_utoa(v, base, want);
  snprintf(ctx, sizeof ctx, "utoa_base(%u, base %u)", v, base);
  return check_conversion(1, (long long)v, base, want);
}

/* ----------------------------- named values ------------------------ */

static void run_named(void) {
  static const int values[] = {
      0,       1,           -1,        5,           9,           10,
      11,      -10,         99,        100,         101,         -100,
      999,     1000,        12345,     -12345,      65535,       65536,
      -65536,  1000000000,  999999999, -1000000000, -999999999,  2000000000,
      INT_MAX, INT_MAX - 1, INT_MIN,   INT_MIN + 1, INT_MIN + 2,
  };
  for (size_t i = 0; i < sizeof values / sizeof values[0]; i++) {
    int ok = check_int(values[i]);
    printf("[%s] int_to_str(%d)\n", ok ? "PASS" : "FAIL", values[i]);
    tally(ok);
  }
}

static void run_powers(void) {
  int before = tests_failed, count = 0;
  long long p = 1;
  for (int k = 0; k <= 9; k++, p *= 10) {
    long long vals[] = {p - 1, p, p + 1, -(p - 1), -p, -(p + 1)};
    for (size_t i = 0; i < sizeof vals / sizeof vals[0]; i++) {
      if (vals[i] > INT_MAX || vals[i] < INT_MIN)
        continue;
      tally(check_int((int)vals[i]));
      count++;
    }
  }
  printf("[%s] every power of ten from 1 to 10^9, and its neighbors, positive "
         "and negative (%d values)\n",
         tests_failed == before ? "PASS" : "FAIL", count);
}

static void run_small_sweep(void) {
  int before = tests_failed, count = 0;
  for (int v = -2000; v <= 2000; v++) {
    tally(check_int(v));
    count++;
  }
  printf("[%s] every int from -2000 to 2000 (%d values)\n",
         tests_failed == before ? "PASS" : "FAIL", count);
}

static void run_random(void) {
  int before = tests_failed;
  srand(8675309);
  const int ITER = 100000;
  for (int i = 0; i < ITER; i++) {
    int v;
    switch (rand() % 4) {
    case 0:
      v = (int)(((unsigned)(rand() & 0xFFFF) << 16) |
                (unsigned)(rand() & 0xFFFF));
      break; /* any bits */
    case 1:
      v = rand() % 1000 - 500;
      break; /* small */
    case 2:
      v = INT_MAX - rand() % 100;
      break; /* near the top */
    default:
      v = INT_MIN + rand() % 100;
      break; /* near the bottom */
    }
    tally(check_int(v));
  }
  printf("[%s] %d random ints, including values near INT_MAX and INT_MIN\n",
         tests_failed == before ? "PASS" : "FAIL", ITER);
}

/* ----------------------------- bases ------------------------------- */

static void run_bases(void) {
  static const struct {
    uint32_t v;
    unsigned base;
    const char *want;
  } vecs[] = {
      {255, 16, "ff"},
      {255, 2, "11111111"},
      {0, 2, "0"},
      {0, 16, "0"},
      {5, 2, "101"},
      {255, 10, "255"},
      {0xFFFFFFFFu, 16, "ffffffff"},
      {0xFFFFFFFFu, 10, "4294967295"},
      {0xFFFFFFFFu, 2, "11111111111111111111111111111111"},
      {0xDEADBEEFu, 16, "deadbeef"},
      {35, 8, "43"},
      {10, 16, "a"},
      {15, 16, "f"},
      {16, 16, "10"},
      {0x80000000u, 16, "80000000"},
  };
  for (size_t i = 0; i < sizeof vecs / sizeof vecs[0]; i++) {
    int ok = check_base(vecs[i].v, vecs[i].base);
    char exp[40];
    ref_utoa(vecs[i].v, vecs[i].base, exp);
    snprintf(ctx, sizeof ctx, "utoa_base(%u, base %u)", vecs[i].v,
             vecs[i].base);
    CHK(strcmp(exp, vecs[i].want) == 0,
        "BAD TEST: reference gave \"%s\", table says \"%s\"", exp,
        vecs[i].want);
    printf("[%s] utoa_base(%u, base %u) = \"%s\"\n", ok ? "PASS" : "FAIL",
           vecs[i].v, vecs[i].base, vecs[i].want);
    tally(ok);
  }

  int before = tests_failed, count = 0;
  srand(1138);
  for (unsigned base = 2; base <= 16; base++)
    for (int i = 0; i < 400; i++) {
      uint32_t v = (i == 0)   ? 0u
                   : (i == 1) ? 0xFFFFFFFFu
                   : (i == 2) ? 0x80000000u
                              : ((uint32_t)(rand() & 0xFFFF) << 16) |
                                    (uint32_t)(rand() & 0xFFFF);
      if (i % 3 == 0 && i > 2)
        v >>= (rand() % 32);
      tally(check_base(v, base));
      count++;
    }
  printf("[%s] every base from 2 to 16, 400 values each (%d cases)\n",
         tests_failed == before ? "PASS" : "FAIL", count);

  before = tests_failed;
  {
    static const unsigned bad[] = {0, 1, 17, 36, 100, 0xFFFFFFFFu};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
      int ok = 1;
      snprintf(ctx, sizeof ctx, "utoa_base with invalid base %u", bad[i]);
      unsigned char b[16];
      memset(b, CANARY, sizeof b);
      size_t r = utoa_base(255, bad[i], (char *)b, sizeof b);
      CHK(r == 0, "returned %zu, expected 0", r);
      tally(ok);
    }
  }
  printf("[%s] invalid bases (0, 1, 17, 36, ...) return 0\n",
         tests_failed == before ? "PASS" : "FAIL");
}

/* ------------------------------ main ------------------------------- */

int main(void) {
  printf("--- int_to_str: named values ---\n");
  run_named();

  printf("\n--- int_to_str: sweeps and random ---\n");
  run_powers();
  run_small_sweep();
  run_random();

  printf("\n--- utoa_base ---\n");
  run_bases();

  if (fail_lines > MAX_FAIL_LINES)
    printf("\n(%d further failure lines suppressed)\n",
           fail_lines - MAX_FAIL_LINES);

  printf("\n%d/%d cases passed\n", tests_run - tests_failed, tests_run);
  return tests_failed != 0;
}
