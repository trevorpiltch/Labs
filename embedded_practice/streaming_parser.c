#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
Problem

Bytes arrive from a serial link one at a time, for example from a UART
interrupt. Write a parser that is fed one byte at a time and reports when a
complete packet has been received.

Packet format
| SYNC (1 byte) | LEN (1 byte) | PAYLOAD (LEN bytes) |
|     0xAA      |   0..16      |        ...          |
SYNC is always 0xAA.
LEN is the payload length. MAX_PAYLOAD is 16, so LEN above 16 is invalid.
There is no checksum in this part.

Rules
Bytes that arrive while waiting for a sync byte (anything other than 0xAA) are
ignored and return PARSE_NONE. After SYNC, the next byte is LEN. If LEN >
MAX_PAYLOAD, return PARSE_DROPPED for that byte and resynchronize. If the
invalid LEN byte is itself 0xAA, treat it as a new sync byte, so the next byte
is a LEN. Otherwise go back to waiting for sync. The next LEN bytes are the
payload, and they are taken literally (a payload byte equal to 0xAA is just
data). The byte that completes the payload returns PARSE_PACKET. LEN == 0 is
valid: the packet completes on the LEN byte itself, with a payload length of 0.
After a packet completes, the parser automatically goes back to waiting for
sync. Results must not depend on how the stream is split across calls, so
feeding the same bytes in the same order always gives the same results.

Constraints
No malloc, no recursion, no global state (two parsers must work independently).
O(1) time per byte, with a fixed-size parser_t.
parser_init must fully reset the parser, and any byte value 0 to 255 is legal
input.
*/

#define SYNC 0xAA
#define MAX_PAYLOAD 16

typedef enum {
  PARSE_NONE,   // byte consumed, no packet completed
  PARSE_PACKET, // this byte completed a valid packet
  PARSE_DROPPED // this byte was an invalid LEN; the parser has resynchronized
} parse_result_t;

typedef enum {
  PARSE_WAIT_FOR_SYNC,
  PARSE_LEN,
  PARSE_PAYLOAD,
  PARSE_CHECKSUM
} parser_state_t;

typedef struct {
  parser_state_t state;
  int payload_len;
  uint8_t payload[MAX_PAYLOAD];
  int current_payload_len;

} parser_t;

void parser_init(parser_t *p) {
  p->state = PARSE_WAIT_FOR_SYNC;
  p->payload_len = 0;
  p->current_payload_len = 0;
}

parse_result_t parser_feed(parser_t *p, uint8_t byte) {
  switch (p->state) {
  case PARSE_WAIT_FOR_SYNC:
    if (byte == SYNC) {
      p->state = PARSE_LEN;
    }

    return PARSE_NONE;
  case PARSE_LEN:
    if (byte == SYNC) {
      p->state = PARSE_LEN;
      return PARSE_DROPPED;
    } else if (byte > MAX_PAYLOAD) {
      p->state = PARSE_WAIT_FOR_SYNC;
      return PARSE_DROPPED;
    } else if (byte == 0) {
      p->current_payload_len = 0;
      p->state = PARSE_WAIT_FOR_SYNC;
      return PARSE_PACKET;
    } else {
      p->current_payload_len = 0;
      p->payload_len = byte;
      p->state = PARSE_PAYLOAD;
      return PARSE_NONE;
    }
  case PARSE_PAYLOAD:
    if (p->current_payload_len < p->payload_len) {
      p->payload[p->current_payload_len] = byte;
      p->current_payload_len++;
    }

    if (p->current_payload_len == p->payload_len) {
      p->state = PARSE_CHECKSUM;
      return PARSE_PACKET;
    } else {
      return PARSE_NONE;
    }
  case PARSE_CHECKSUM:
    uint8_t crc = 0;
    crc ^= p->payload_len;
    for (int i = 0; i < p->current_payload_len; i++) {
      crc ^= p->payload[i];
    }

    p->state = PARSE_WAIT_FOR_SYNC;

    if (crc != byte) {
      return PARSE_DROPPED;
    } else {
      return PARSE_PACKET;
    }
  }
}

// Valid only immediately after parser_feed returned PARSE_PACKET,
// and only until the next call to parser_feed.
size_t parser_payload_len(const parser_t *p) {
  return (size_t)p->current_payload_len;
}
const uint8_t *parser_payload(const parser_t *p) { return p->payload; }

/* ------------------------------------------------------------------ */
/* Test harness                                                        */
/* ------------------------------------------------------------------ */

#define MAX_FAIL_LINES 40

static char ctx[160]; /* describes the case currently running */
static int fail_lines =
    0; /* failure lines printed so far (printing is capped) */

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

static char res_char(parse_result_t r) {
  switch (r) {
  case PARSE_NONE:
    return 'n';
  case PARSE_PACKET:
    return 'P';
  case PARSE_DROPPED:
    return 'D';
  default:
    return '?';
  }
}

static const char *char_name(char c) {
  switch (c) {
  case 'n':
    return "NONE";
  case 'P':
    return "PACKET";
  case 'D':
    return "DROPPED";
  default:
    return "(invalid result value)";
  }
}

typedef struct {
  uint8_t len;
  uint8_t data[MAX_PAYLOAD];
} pkt_t;

/* Feed ONE byte and check the result (and the payload if a packet completes).
 */
static int check_byte(parser_t *p, uint8_t b, size_t idx, char want,
                      const pkt_t *pk, size_t n_pk, size_t *seen) {
  int ok = 1;
  parse_result_t r = parser_feed(p, b);
  char got = res_char(r);

  CHK(got == want, "byte %zu (0x%02X): got %s, expected %s", idx, b,
      char_name(got), char_name(want));

  if (r == PARSE_PACKET) {
    size_t len = parser_payload_len(p);
    const uint8_t *data = parser_payload(p);
    if (*seen >= n_pk) {
      CHK(0, "byte %zu: unexpected extra packet", idx);
    } else {
      const pkt_t *e = &pk[*seen];
      CHK(len == e->len, "packet %zu: payload_len %zu, expected %u", *seen, len,
          e->len);
      if (len == e->len && len > 0) {
        CHK(data != NULL, "packet %zu: payload pointer is NULL", *seen);
        if (data)
          CHK(memcmp(data, e->data, len) == 0,
              "packet %zu: payload bytes differ", *seen);
      }
    }
    (*seen)++;
  }
  return ok;
}

/* Feed a whole stream and compare the per-byte results and every completed
 * packet. */
static int feed_and_compare(parser_t *p, const uint8_t *in, size_t n,
                            const char *results, const pkt_t *pk, size_t n_pk) {
  int ok = 1;
  size_t seen = 0;
  for (size_t i = 0; i < n; i++)
    if (!check_byte(p, in[i], i, results[i], pk, n_pk, &seen))
      ok = 0;
  CHK(seen == n_pk, "saw %zu packets, expected %zu", seen, n_pk);
  return ok;
}

static int tests_run = 0, tests_failed = 0;

static void tally(int ok) {
  tests_run++;
  if (!ok)
    tests_failed++;
}

/* ------------------- the hand-written examples --------------------- */

typedef struct {
  const char *name;
  uint8_t in[24];
  size_t in_len;
  const char
      *results; /* one char per input byte: n = NONE, P = PACKET, D = DROPPED */
  size_t n_pk;
  pkt_t pk[3]; /* expected packets, in order */
} case_t;

static const case_t examples[] = {
    {"valid packet",
     {0xAA, 0x03, 0x01, 0x02, 0x03},
     5,
     "nnnnP",
     1,
     {{3, {0x01, 0x02, 0x03}}}},

    {"noise, then a zero-length packet",
     {0x00, 0x11, 0xAA, 0x00},
     4,
     "nnnP",
     1,
     {{0, {0}}}},

    {"two packets back to back",
     {0xAA, 0x01, 0x7F, 0xAA, 0x02, 0x11, 0x22},
     7,
     "nnPnnnP",
     2,
     {{1, {0x7F}}, {2, {0x11, 0x22}}}},

    {"0xAA inside the payload is just data",
     {0xAA, 0x03, 0xAA, 0x01, 0xAA},
     5,
     "nnnnP",
     1,
     {{3, {0xAA, 0x01, 0xAA}}}},

    {"invalid LEN (0xFF), then noise",
     {0xAA, 0xFF, 0x00},
     3,
     "nDn",
     0,
     {{0, {0}}}},

    {"invalid LEN that is itself 0xAA counts as a new sync",
     {0xAA, 0xAA, 0x01, 0x55},
     4,
     "nDnP",
     1,
     {{1, {0x55}}}},

    {"LEN = 17 is invalid (MAX_PAYLOAD + 1)",
     {0xAA, 0x11},
     2,
     "nD",
     0,
     {{0, {0}}}},

    {"LEN = 16 is valid (MAX_PAYLOAD)",
     {0xAA, 0x10, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16},
     18,
     "nnnnnnnnnnnnnnnnnP",
     1,
     {{16, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}}}},

    {"garbage between two packets",
     {0xAA, 0x01, 0x11, 0x00, 0x00, 0xAA, 0x01, 0x22},
     8,
     "nnPnnnnP",
     2,
     {{1, {0x11}}, {1, {0x22}}}},

    {"sync byte only (nothing completes)", {0xAA}, 1, "n", 0, {{0, {0}}}},

    {"truncated packet (nothing completes)",
     {0xAA, 0x05, 0x01, 0x02},
     4,
     "nnnn",
     0,
     {{0, {0}}}},

    {"invalid LEN (0x20), then a valid packet right away",
     {0xAA, 0x20, 0xAA, 0x01, 0x33},
     5,
     "nDnnP",
     1,
     {{1, {0x33}}}},

    {"pure noise", {0x00, 0x01, 0x02, 0xFF, 0x55}, 5, "nnnnn", 0, {{0, {0}}}},

    {"three syncs in a row, then LEN 0",
     {0xAA, 0xAA, 0xAA, 0x00},
     4,
     "nDDP",
     1,
     {{0, {0}}}},
};

static void run_examples(void) {
  for (size_t i = 0; i < sizeof examples / sizeof examples[0]; i++) {
    const case_t *c = &examples[i];
    if (strlen(c->results) != c->in_len) {
      printf("BAD TEST '%s': results string length %zu != input length %zu\n",
             c->name, strlen(c->results), c->in_len);
      exit(2);
    }
    snprintf(ctx, sizeof ctx, "%s", c->name);
    parser_t p;
    parser_init(&p);
    int ok = feed_and_compare(&p, c->in, c->in_len, c->results, c->pk, c->n_pk);
    printf("[%s] %s\n", ok ? "PASS" : "FAIL", c->name);
    tally(ok);
  }
}

/* ------------------------- sweep tests ----------------------------- */

/* A valid packet with every LEN 0..16, sent twice back to back (also checks
 * re-arming). */
static int test_len_sweep(unsigned len) {
  int ok = 1;
  uint8_t in[2 * (2 + MAX_PAYLOAD)];
  char res[2 * (2 + MAX_PAYLOAD) + 1];
  pkt_t pk[2];
  size_t n = 0;

  for (int rep = 0; rep < 2; rep++) {
    in[n] = SYNC;
    res[n++] = 'n';
    in[n] = (uint8_t)len;
    res[n++] = (len == 0) ? 'P' : 'n';
    pk[rep].len = (uint8_t)len;
    memset(pk[rep].data, 0, sizeof pk[rep].data);
    for (unsigned i = 0; i < len; i++) {
      uint8_t v = (uint8_t)(i * 7u + len + 1u);
      in[n] = v;
      pk[rep].data[i] = v;
      res[n++] = (i == len - 1) ? 'P' : 'n';
    }
  }
  res[n] = '\0';

  snprintf(ctx, sizeof ctx, "valid LEN = %u (two packets back to back)", len);
  parser_t p;
  parser_init(&p);
  CHK(feed_and_compare(&p, in, n, res, pk, 2), "stream failed");
  return ok;
}

/* Every invalid LEN 17..255 must be DROPPED, and the parser must then
 * resynchronize. */
static int test_invalid_len(unsigned len) {
  int ok = 1;
  snprintf(ctx, sizeof ctx, "invalid LEN = 0x%02X", len);

  parser_t p;
  parser_init(&p);

  if (len == SYNC) {
    /* AA AA 01 55: the invalid LEN is also a sync, so 01 is a LEN and 55 its
     * payload */
    const uint8_t in[] = {SYNC, SYNC, 0x01, 0x55};
    const pkt_t pk[1] = {{1, {0x55}}};
    CHK(feed_and_compare(&p, in, sizeof in, "nDnP", pk, 1), "stream failed");
  } else {
    /* AA <len> AA 00: parser must be waiting for sync again, so AA 00 is a
     * zero-length packet */
    const uint8_t in[] = {SYNC, (uint8_t)len, SYNC, 0x00};
    const pkt_t pk[1] = {{0, {0}}};
    CHK(feed_and_compare(&p, in, sizeof in, "nDnP", pk, 1), "stream failed");
  }
  return ok;
}

/* Every non-sync byte must be ignored while waiting for sync. */
static int test_noise_byte(unsigned b) {
  int ok = 1;
  snprintf(ctx, sizeof ctx, "noise byte 0x%02X while waiting for sync", b);

  const uint8_t in[] = {(uint8_t)b, (uint8_t)b, SYNC, 0x00};
  const pkt_t pk[1] = {{0, {0}}};
  parser_t p;
  parser_init(&p);
  CHK(feed_and_compare(&p, in, sizeof in, "nnnP", pk, 1), "stream failed");
  return ok;
}

/* ------------------ independence and reset tests ------------------- */

/* Two parsers fed in an interleaved fashion must not affect each other. */
static int test_independent(void) {
  int ok = 1;
  snprintf(ctx, sizeof ctx, "two parsers interleaved");

  const uint8_t sa[] = {SYNC, 0x03, 0x01, 0x02, 0x03};
  const char *ra = "nnnnP";
  const pkt_t pa[1] = {{3, {0x01, 0x02, 0x03}}};

  const uint8_t sb[] = {SYNC, 0x01, 0x7F};
  const char *rb = "nnP";
  const pkt_t pb[1] = {{1, {0x7F}}};

  parser_t a, b;
  parser_init(&a);
  parser_init(&b);
  size_t seen_a = 0, seen_b = 0;

  for (size_t i = 0; i < sizeof sa; i++) {
    if (!check_byte(&a, sa[i], i, ra[i], pa, 1, &seen_a))
      ok = 0;
    if (i < sizeof sb)
      if (!check_byte(&b, sb[i], i, rb[i], pb, 1, &seen_b))
        ok = 0;
  }
  CHK(seen_a == 1 && seen_b == 1, "packets seen: A=%zu B=%zu, expected 1 each",
      seen_a, seen_b);
  return ok;
}

/* parser_init in the middle of a payload must discard the partial packet. */
static int test_reset_mid_payload(void) {
  int ok = 1;
  snprintf(ctx, sizeof ctx, "parser_init while in the middle of a payload");

  parser_t p;
  parser_init(&p);
  const uint8_t part[] = {SYNC, 0x05, 0x01, 0x02};
  CHK(feed_and_compare(&p, part, sizeof part, "nnnn", NULL, 0),
      "partial packet failed");

  parser_init(&p);
  const uint8_t again[] = {SYNC, 0x00};
  const pkt_t pk[1] = {{0, {0}}};
  CHK(feed_and_compare(&p, again, sizeof again, "nP", pk, 1),
      "after reset, AA 00 should be a zero-length packet");
  return ok;
}

/* parser_init right after a sync byte must forget that sync. */
static int test_reset_after_sync(void) {
  int ok = 1;
  snprintf(ctx, sizeof ctx, "parser_init right after a sync byte");

  parser_t p;
  parser_init(&p);
  const uint8_t first[] = {SYNC};
  CHK(feed_and_compare(&p, first, sizeof first, "n", NULL, 0),
      "sync byte failed");

  parser_init(&p);
  /* 03 is noise now (not a LEN), then AA 00 is a zero-length packet */
  const uint8_t again[] = {0x03, SYNC, 0x00};
  const pkt_t pk[1] = {{0, {0}}};
  CHK(feed_and_compare(&p, again, sizeof again, "nnP", pk, 1),
      "stale sync state survived parser_init");
  return ok;
}

/* ------------------ reference parser + random streams -------------- */

typedef struct {
  int state; /* 0 = wait sync, 1 = wait LEN, 2 = read payload */
  uint8_t len, count;
  uint8_t buf[MAX_PAYLOAD];
} ref_t;

static void ref_init(ref_t *r) { memset(r, 0, sizeof *r); }

static parse_result_t ref_feed(ref_t *r, uint8_t b) {
  switch (r->state) {
  case 0:
    if (b == SYNC)
      r->state = 1;
    return PARSE_NONE;
  case 1:
    if (b > MAX_PAYLOAD) {
      r->state = (b == SYNC) ? 1 : 0;
      return PARSE_DROPPED;
    }
    r->len = b;
    r->count = 0;
    if (b == 0) {
      r->state = 0;
      return PARSE_PACKET;
    }
    r->state = 2;
    return PARSE_NONE;
  default:
    r->buf[r->count++] = b;
    if (r->count == r->len) {
      r->state = 0;
      return PARSE_PACKET;
    }
    return PARSE_NONE;
  }
}

static uint8_t random_byte(void) {
  int r = rand() % 100;
  if (r < 25)
    return SYNC; /* lots of sync bytes */
  if (r < 55)
    return (uint8_t)(rand() %
                     20); /* small values: valid LENs, 17..19 invalid */
  if (r < 60)
    return 0xFF;
  return (uint8_t)(rand() % 256);
}

static int test_random_stream(void) {
  int ok = 1;
  uint8_t in[200];
  size_t n = 1 + (size_t)(rand() % 200);

  parser_t p;
  ref_t r;
  parser_init(&p);
  ref_init(&r);

  for (size_t i = 0; i < n; i++) {
    in[i] = random_byte();
    parse_result_t want = ref_feed(&r, in[i]);
    parse_result_t got = parser_feed(&p, in[i]);

    int bad = (got != want);
    if (!bad && want == PARSE_PACKET) {
      size_t len = parser_payload_len(&p);
      const uint8_t *data = parser_payload(&p);
      bad = (len != r.len) ||
            (len > 0 && (data == NULL || memcmp(data, r.buf, len) != 0));
    }

    if (bad) {
      snprintf(ctx, sizeof ctx, "random stream");
      CHK(0, "byte %zu (0x%02X): got %s, expected %s (or payload mismatch)", i,
          in[i], char_name(res_char(got)), char_name(res_char(want)));
      if (fail_lines <= MAX_FAIL_LINES) {
        printf("    stream so far:");
        for (size_t k = 0; k <= i && k < 48; k++)
          printf(" %02X", in[k]);
        printf("%s\n", i >= 48 ? " ..." : "");
      }
      return 0;
    }
  }
  return ok;
}

/* ------------------------------ main ------------------------------- */

int main(void) {
  int before;

  printf("--- examples ---\n");
  run_examples();

  printf("\n--- sweeps ---\n");
  before = tests_failed;
  for (unsigned len = 0; len <= MAX_PAYLOAD; len++)
    tally(test_len_sweep(len));
  printf("[%s] every valid LEN 0..16, two packets back to back (17 cases)\n",
         tests_failed == before ? "PASS" : "FAIL");

  before = tests_failed;
  for (unsigned len = MAX_PAYLOAD + 1; len <= 255; len++)
    tally(test_invalid_len(len));
  printf(
      "[%s] every invalid LEN 17..255 drops and resynchronizes (239 cases)\n",
      tests_failed == before ? "PASS" : "FAIL");

  before = tests_failed;
  int noise_cases = 0;
  for (unsigned b = 0; b <= 255; b++) {
    if (b == SYNC)
      continue;
    tally(test_noise_byte(b));
    noise_cases++;
  }
  printf(
      "[%s] every non-sync byte is ignored while waiting for sync (%d cases)\n",
      tests_failed == before ? "PASS" : "FAIL", noise_cases);

  printf("\n--- state handling ---\n");
  before = tests_failed;
  int ok_a = test_independent();
  tally(ok_a);
  printf("[%s] two parsers do not share state\n", ok_a ? "PASS" : "FAIL");
  int ok_b = test_reset_mid_payload();
  tally(ok_b);
  printf("[%s] parser_init resets a half-received packet\n",
         ok_b ? "PASS" : "FAIL");
  int ok_c = test_reset_after_sync();
  tally(ok_c);
  printf("[%s] parser_init forgets a pending sync byte\n",
         ok_c ? "PASS" : "FAIL");

  printf("\n--- random streams vs. reference parser ---\n");
  before = tests_failed;
  srand(2024);
  const int ITER = 20000;
  for (int i = 0; i < ITER; i++)
    tally(test_random_stream());
  printf("[%s] %d random streams\n", tests_failed == before ? "PASS" : "FAIL",
         ITER);

  if (fail_lines > MAX_FAIL_LINES)
    printf("\n(%d further failure lines suppressed)\n",
           fail_lines - MAX_FAIL_LINES);

  printf("\n%d/%d cases passed\n", tests_run - tests_failed, tests_run);
  return tests_failed != 0;
}
