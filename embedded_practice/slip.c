/*
 * Test script for byte stuffing (SLIP-style framing).
 *
 * Build and run:
 *   gcc -std=c11 -Wall -Wextra -g -O0 -fsanitize=address,undefined test_slip.c
 * -o test_slip && ./test_slip
 *
 * Special bytes:
 *   END = 0xC0   frame delimiter
 *   ESC = 0xDB   escape
 *   ESC_END = 0xDC, ESC_ESC = 0xDD   what follows ESC to stand for END / ESC in
 * the data
 *
 * ENCODER   size_t slip_encode(const uint8_t *in, size_t in_len, uint8_t *out,
 * size_t out_cap); Writes:  END, then every input byte (0xC0 becomes DB DC,
 * 0xDB becomes DB DD, everything else is copied unchanged), then END. Returns
 * the number of bytes written. If the whole frame does not fit in out_cap,
 * returns 0 and must not write past out_cap. An empty input encodes to C0 C0.
 *
 * DECODER (streaming, one byte at a time)
 *   void           slip_decoder_init(slip_decoder_t *d);
 *   slip_result_t  slip_decoder_feed(slip_decoder_t *d, uint8_t byte);
 *   size_t         slip_frame_len(const slip_decoder_t *d);
 *   const uint8_t *slip_frame(const slip_decoder_t *d);      // valid only
 * right after SLIP_FRAME
 *
 *   Results:  SLIP_NONE (nothing to report), SLIP_FRAME (this END byte
 * completed a frame), SLIP_ERROR (this byte exposed a protocol error). Rules:
 *     1. END ends the current frame. If the frame has no data bytes (END END,
 * or a leading END), it is ignored: return SLIP_NONE. A frame needs no leading
 * END; data before the first END simply belongs to the first frame. A single
 * END may serve as both the end of one frame and the start of the next.
 *     2. ESC followed by DC stands for C0, and ESC followed by DD stands for
 * DB. A bare DC or DD (not preceded by ESC) is ordinary data.
 *     3. ESC followed by any other byte is a protocol error: return SLIP_ERROR
 * on THAT byte and throw the frame away. If that byte is END, a new frame
 * starts right there. Otherwise discard bytes (returning SLIP_NONE) until the
 * next END, which starts a new frame (no result).
 *     4. A frame may hold at most SLIP_MAX_FRAME (32) decoded bytes. One more
 * returns SLIP_ERROR on the byte that would be the 33rd (for an escape, the
 * second byte of the pair), the frame is thrown away, and bytes are discarded
 * until the next END, which starts a new frame.
 *     5. No malloc and no global state; two decoders must work independently.
 *
 * What the tests do:
 *   - encoder vectors, exact / roomy / too-small / zero capacity, never writes
 * past out_cap
 *   - decoder examples (each printed by name), including errors, overflow and
 * recovery
 *   - encode then decode round trips (single frames and several frames in one
 * stream)
 *   - every byte value as data, and every byte value after ESC
 *   - two decoders interleaved, and slip_decoder_init resetting mid-frame
 *   - 20,000 random streams, heavy on C0/DB/DC/DD, compared byte-for-byte with
 * a reference decoder
 *
 * Replace the stub section with your implementation (or #include your own file
 * instead).
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SLIP_END 0xC0
#define SLIP_ESC 0xDB
#define SLIP_ESC_END 0xDC
#define SLIP_ESC_ESC 0xDD
#define SLIP_MAX_FRAME 32

typedef enum { SLIP_NONE, SLIP_FRAME, SLIP_ERROR } slip_result_t;

/* ------------------------------------------------------------------ */
/* TODO: your encoder and decoder go here.                             */
/* ------------------------------------------------------------------ */

size_t slip_encode(const uint8_t *in, size_t in_len, uint8_t *out,
                   size_t out_cap) {

  size_t i = 0;
  size_t i_out = 1;

  if (out_cap < 2) {
    return 0;
  }

  out[0] = SLIP_END;

  while (i < in_len) {
    if (in[i] == SLIP_END) {
      if (i_out + 2 >= out_cap) {
        return 0;
      }
      out[i_out] = SLIP_ESC;
      out[i_out + 1] = SLIP_ESC_END;
      i_out++;
    } else if (in[i] == SLIP_ESC) {
      if (i_out + 2 >= out_cap) {
        return 0;
      }
      out[i_out] = SLIP_ESC;
      out[i_out + 1] = SLIP_ESC_ESC;
      i_out++;
    } else {
      if (i_out + 1 >= out_cap) {
        return 0;
      }
      out[i_out] = in[i];
    }

    i++;
    i_out++;
  }

  out[i_out] = SLIP_END;
  i_out++;

  return i_out;
}

typedef enum { PARSED_ESC, PARSING, WAIT_FOR_END } slip_state_t;

typedef struct {
  slip_state_t state;
  uint8_t buff[SLIP_MAX_FRAME];
  uint8_t frame[SLIP_MAX_FRAME];
  size_t curr_len;
  size_t frame_len;
} slip_decoder_t;

void slip_decoder_init(slip_decoder_t *d) {
  d->state = PARSING;
  d->curr_len = 0;
  d->frame_len = 0;
}

slip_result_t slip_decoder_feed(slip_decoder_t *d, uint8_t byte) {
  switch (d->state) {
  case PARSING:
    if (byte == SLIP_END) {
      if (d->curr_len == 0) {
        return SLIP_NONE;
      }
      memcpy(d->frame, d->buff, d->curr_len);
      d->frame_len = d->curr_len;
      d->curr_len = 0;
      return SLIP_FRAME;
    }

    if (byte == SLIP_ESC) {
      d->state = PARSED_ESC;
      return SLIP_NONE;
    }

    if (d->curr_len >= SLIP_MAX_FRAME) {
      d->state = WAIT_FOR_END;
      d->curr_len = 0;
      return SLIP_ERROR;
    }

    d->buff[d->curr_len++] = byte;
    return SLIP_NONE;
  case PARSED_ESC:
    if (byte == SLIP_ESC_END || byte == SLIP_ESC_ESC) {
      if (d->curr_len >= SLIP_MAX_FRAME) {
        d->state = WAIT_FOR_END;
        d->curr_len = 0;
        return SLIP_ERROR;
      }
      if (byte == SLIP_ESC_END) {
        d->buff[d->curr_len++] = 0xC0;
      } else if (byte == SLIP_ESC_ESC) {
        d->buff[d->curr_len++] = 0xDB;
      }

      d->state = PARSING;
      return SLIP_NONE;
    }

    if (byte == SLIP_END) {
      d->state = PARSING;
    } else {
      d->state = WAIT_FOR_END;
    }
    d->curr_len = 0;
    return SLIP_ERROR;
  case WAIT_FOR_END:
    if (byte == SLIP_END) {
      d->state = PARSING;
      d->curr_len = 0;
    }

    return SLIP_NONE;
  }
}

size_t slip_frame_len(const slip_decoder_t *d) { return d->frame_len; }

const uint8_t *slip_frame(const slip_decoder_t *d) { return d->frame; }

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

static uint8_t *dup_exact(const uint8_t *src, size_t n) {
  uint8_t *p = xmalloc(n);
  if (n)
    memcpy(p, src, n);
  return p;
}

static int all_equal(const uint8_t *p, size_t n, uint8_t v) {
  for (size_t i = 0; i < n; i++)
    if (p[i] != v)
      return 0;
  return 1;
}

static const char *hex(const uint8_t *p, size_t n) {
  static char buf[4][128];
  static int k = 0;
  char *s = buf[k++ & 3];
  size_t pos = 0;
  s[0] = '\0';
  for (size_t i = 0; i < n && pos + 4 < 120; i++)
    pos += (size_t)snprintf(s + pos, 128 - pos, "%02X ", p[i]);
  if (n && pos > 0)
    s[pos - 1] = '\0';
  if (n * 3 > 118)
    strcpy(s + 112, " ...");
  return s;
}

/* ------------------------- reference encoder ----------------------- */

static size_t ref_encode(const uint8_t *in, size_t n, uint8_t *out,
                         size_t cap) {
  size_t need = 2;
  for (size_t i = 0; i < n; i++)
    need += (in[i] == SLIP_END || in[i] == SLIP_ESC) ? 2 : 1;
  if (need > cap)
    return 0;
  size_t o = 0;
  out[o++] = SLIP_END;
  for (size_t i = 0; i < n; i++) {
    if (in[i] == SLIP_END) {
      out[o++] = SLIP_ESC;
      out[o++] = SLIP_ESC_END;
    } else if (in[i] == SLIP_ESC) {
      out[o++] = SLIP_ESC;
      out[o++] = SLIP_ESC_ESC;
    } else {
      out[o++] = in[i];
    }
  }
  out[o++] = SLIP_END;
  return o;
}

/* ------------------------ reference decoder ------------------------ */

typedef struct {
  int state; /* 0 = normal, 1 = after ESC, 2 = discarding until END */
  size_t len;
  uint8_t buf[SLIP_MAX_FRAME];
  size_t frame_len;
  uint8_t frame[SLIP_MAX_FRAME];
} ref_dec_t;

static void ref_init(ref_dec_t *r) { memset(r, 0, sizeof *r); }

static slip_result_t ref_feed(ref_dec_t *r, uint8_t b) {
  switch (r->state) {
  case 2:
    if (b == SLIP_END) {
      r->state = 0;
      r->len = 0;
    }
    return SLIP_NONE;
  case 1:
    if (b == SLIP_ESC_END || b == SLIP_ESC_ESC) {
      if (r->len >= SLIP_MAX_FRAME) {
        r->state = 2;
        r->len = 0;
        return SLIP_ERROR;
      }
      r->buf[r->len++] = (b == SLIP_ESC_END) ? SLIP_END : SLIP_ESC;
      r->state = 0;
      return SLIP_NONE;
    }
    r->state = (b == SLIP_END) ? 0 : 2;
    r->len = 0;
    return SLIP_ERROR;
  default:
    if (b == SLIP_END) {
      if (r->len == 0)
        return SLIP_NONE;
      memcpy(r->frame, r->buf, r->len);
      r->frame_len = r->len;
      r->len = 0;
      return SLIP_FRAME;
    }
    if (b == SLIP_ESC) {
      r->state = 1;
      return SLIP_NONE;
    }
    if (r->len >= SLIP_MAX_FRAME) {
      r->state = 2;
      r->len = 0;
      return SLIP_ERROR;
    }
    r->buf[r->len++] = b;
    return SLIP_NONE;
  }
}

/* ------------------------------ encoder tests ---------------------- */

typedef struct {
  const char *name;
  uint8_t in[8];
  size_t in_len;
  uint8_t exp[18];
  size_t exp_len;
} enc_case_t;

static const enc_case_t enc_cases[] = {
    {"empty input", {0}, 0, {0xC0, 0xC0}, 2},
    {"plain bytes", {0x01, 0x02, 0x03}, 3, {0xC0, 0x01, 0x02, 0x03, 0xC0}, 5},
    {"END (C0) is escaped", {0xC0}, 1, {0xC0, 0xDB, 0xDC, 0xC0}, 4},
    {"ESC (DB) is escaped", {0xDB}, 1, {0xC0, 0xDB, 0xDD, 0xC0}, 4},
    {"C0 then DB", {0xC0, 0xDB}, 2, {0xC0, 0xDB, 0xDC, 0xDB, 0xDD, 0xC0}, 6},
    {"DC and DD alone are data", {0xDC, 0xDD}, 2, {0xC0, 0xDC, 0xDD, 0xC0}, 4},
    {"DB DC in the data", {0xDB, 0xDC}, 2, {0xC0, 0xDB, 0xDD, 0xDC, 0xC0}, 5},
    {"two ENDs", {0xC0, 0xC0}, 2, {0xC0, 0xDB, 0xDC, 0xDB, 0xDC, 0xC0}, 6},
    {"mixed bytes",
     {0x01, 0xC0, 0x02, 0xDB, 0x03},
     5,
     {0xC0, 0x01, 0xDB, 0xDC, 0x02, 0xDB, 0xDD, 0x03, 0xC0},
     9},
    {"all special bytes",
     {0xC0, 0xDB, 0xDC, 0xDD},
     4,
     {0xC0, 0xDB, 0xDC, 0xDB, 0xDD, 0xDC, 0xDD, 0xC0},
     8},
};

static int test_encode_case(const enc_case_t *c) {
  int ok = 1;
  snprintf(ctx, sizeof ctx, "encode: %s", c->name);
  uint8_t *in = dup_exact(c->in, c->in_len);

  /* 1. exact capacity */
  uint8_t *out = xmalloc(c->exp_len);
  size_t r = slip_encode(in, c->in_len, out, c->exp_len);
  CHK(r == c->exp_len, "exact capacity: returned %zu, expected %zu", r,
      c->exp_len);
  if (r == c->exp_len)
    CHK(memcmp(out, c->exp, c->exp_len) == 0, "bytes: got [%s], expected [%s]",
        hex(out, c->exp_len), hex(c->exp, c->exp_len));
  free(out);

  /* 2. roomy capacity: only the frame may be written */
  size_t cap = c->exp_len + 6;
  out = xmalloc(cap + TAIL);
  memset(out, CANARY, cap + TAIL);
  r = slip_encode(in, c->in_len, out, cap);
  CHK(r == c->exp_len, "roomy capacity: returned %zu, expected %zu", r,
      c->exp_len);
  CHK(all_equal(out + c->exp_len, cap + TAIL - c->exp_len, CANARY),
      "wrote beyond the frame");
  free(out);

  /* 3. EVERY capacity that is too small: return 0, never write past out_cap */
  for (cap = 1; cap < c->exp_len; cap++) {
    out = xmalloc(cap + TAIL);
    memset(out, CANARY, cap + TAIL);
    r = slip_encode(in, c->in_len, out, cap);
    CHK(r == 0, "capacity %zu of %zu needed: returned %zu, expected 0", cap,
        c->exp_len, r);
    CHK(all_equal(out + cap, TAIL, CANARY),
        "wrote past out_cap (capacity %zu of %zu needed) when the frame did "
        "not fit",
        cap, c->exp_len);
    free(out);
  }

  /* 4. zero capacity */
  out = xmalloc(TAIL);
  memset(out, CANARY, TAIL);
  r = slip_encode(in, c->in_len, out, 0);
  CHK(r == 0, "zero capacity: returned %zu, expected 0", r);
  CHK(all_equal(out, TAIL, CANARY), "wrote with out_cap == 0");
  free(out);

  free(in);
  return ok;
}

static uint8_t special_byte(void) {
  static const uint8_t sp[] = {SLIP_END, SLIP_ESC, SLIP_ESC_END, SLIP_ESC_ESC};
  return sp[rand() % 4];
}

static uint8_t random_data_byte(void) {
  return (rand() % 100 < 40) ? special_byte() : (uint8_t)(rand() & 0xFF);
}

static int test_encode_random(void) {
  int ok = 1;
  size_t n = (size_t)(rand() % 65);
  uint8_t tmp[64], expect[2 * 64 + 2];
  for (size_t i = 0; i < n; i++)
    tmp[i] = random_data_byte();
  uint8_t *in = dup_exact(tmp, n);

  snprintf(ctx, sizeof ctx, "encode random, len=%zu", n);
  size_t want_len = ref_encode(in, n, expect, sizeof expect);

  uint8_t *out = xmalloc(want_len);
  size_t got = slip_encode(in, n, out, want_len);
  CHK(got == want_len, "returned %zu, expected %zu", got, want_len);
  if (got == want_len)
    CHK(memcmp(out, expect, want_len) == 0, "got [%s], expected [%s]",
        hex(out, want_len), hex(expect, want_len));

  /* structure: END appears only at the first and last position; length = n +
   * specials + 2 */
  if (got == want_len && got >= 2) {
    size_t specials = 0, ends = 0;
    for (size_t i = 0; i < n; i++)
      if (in[i] == SLIP_END || in[i] == SLIP_ESC)
        specials++;
    for (size_t i = 0; i < got; i++)
      if (out[i] == SLIP_END)
        ends++;
    CHK(ends == 2 && out[0] == SLIP_END && out[got - 1] == SLIP_END,
        "END (0xC0) must appear exactly twice, at the start and end");
    CHK(got == n + specials + 2, "length %zu, expected %zu", got,
        n + specials + 2);
  }
  free(out);
  free(in);
  return ok;
}

/* ------------------------------ decoder tests ---------------------- */

typedef struct {
  size_t len;
  uint8_t data[SLIP_MAX_FRAME];
} frame_t;

static char res_char(slip_result_t r) {
  switch (r) {
  case SLIP_NONE:
    return 'n';
  case SLIP_FRAME:
    return 'F';
  case SLIP_ERROR:
    return 'E';
  default:
    return '?';
  }
}

static const char *char_name(char c) {
  switch (c) {
  case 'n':
    return "NONE";
  case 'F':
    return "FRAME";
  case 'E':
    return "ERROR";
  default:
    return "(invalid result value)";
  }
}

static int check_byte(slip_decoder_t *d, uint8_t b, size_t idx, char want,
                      const frame_t *fr, size_t n_fr, size_t *seen) {
  int ok = 1;
  slip_result_t r = slip_decoder_feed(d, b);
  char got = res_char(r);

  CHK(got == want, "byte %zu (0x%02X): got %s, expected %s", idx, b,
      char_name(got), char_name(want));

  if (r == SLIP_FRAME) {
    size_t len = slip_frame_len(d);
    const uint8_t *data = slip_frame(d);
    if (*seen >= n_fr) {
      CHK(0, "byte %zu: unexpected extra frame", idx);
    } else {
      const frame_t *e = &fr[*seen];
      CHK(len == e->len, "frame %zu: length %zu, expected %zu", *seen, len,
          e->len);
      if (len == e->len && len > 0) {
        CHK(data != NULL, "frame %zu: frame pointer is NULL", *seen);
        if (data)
          CHK(memcmp(data, e->data, len) == 0,
              "frame %zu: got [%s], expected [%s]", *seen, hex(data, len),
              hex(e->data, len));
      }
    }
    (*seen)++;
  }
  return ok;
}

static int feed_and_compare(slip_decoder_t *d, const uint8_t *in, size_t n,
                            const char *results, const frame_t *fr,
                            size_t n_fr) {
  int ok = 1;
  size_t seen = 0;
  for (size_t i = 0; i < n; i++)
    if (!check_byte(d, in[i], i, results[i], fr, n_fr, &seen))
      ok = 0;
  CHK(seen == n_fr, "saw %zu frames, expected %zu", seen, n_fr);
  return ok;
}

typedef struct {
  const char *name;
  uint8_t in[24];
  size_t in_len;
  const char
      *results; /* n = NONE, F = FRAME, E = ERROR, one char per input byte */
  size_t n_fr;
  frame_t fr[3];
} dec_case_t;

static const dec_case_t dec_cases[] = {
    {"simple frame (leading and trailing END)",
     {0xC0, 0x01, 0x02, 0x03, 0xC0},
     5,
     "nnnnF",
     1,
     {{3, {0x01, 0x02, 0x03}}}},

    {"no leading END", {0x01, 0x02, 0xC0}, 3, "nnF", 1, {{2, {0x01, 0x02}}}},

    {"escaped END", {0xC0, 0xDB, 0xDC, 0xC0}, 4, "nnnF", 1, {{1, {0xC0}}}},

    {"escaped ESC", {0xC0, 0xDB, 0xDD, 0xC0}, 4, "nnnF", 1, {{1, {0xDB}}}},

    {"bare DC is data", {0xC0, 0xDC, 0xC0}, 3, "nnF", 1, {{1, {0xDC}}}},

    {"bare DD is data", {0xC0, 0xDD, 0xC0}, 3, "nnF", 1, {{1, {0xDD}}}},

    {"mixed escapes",
     {0xC0, 0x01, 0xDB, 0xDC, 0x02, 0xDB, 0xDD, 0x03, 0xC0},
     9,
     "nnnnnnnnF",
     1,
     {{5, {0x01, 0xC0, 0x02, 0xDB, 0x03}}}},

    {"empty frames are ignored", {0xC0, 0xC0, 0xC0}, 3, "nnn", 0, {{0, {0}}}},

    {"frames sharing an END",
     {0xC0, 0x01, 0xC0, 0x02, 0xC0},
     5,
     "nnFnF",
     2,
     {{1, {0x01}}, {1, {0x02}}}},

    {"two frames, doubled END between",
     {0xC0, 0x01, 0xC0, 0xC0, 0x02, 0xC0},
     6,
     "nnFnnF",
     2,
     {{1, {0x01}}, {1, {0x02}}}},

    {"invalid escape (DB 01), then recovery at the next END",
     {0xC0, 0xDB, 0x01, 0x02, 0xC0, 0x03, 0xC0},
     7,
     "nnEnnnF",
     1,
     {{1, {0x03}}}},

    {"escape followed by END: error, and that END starts a new frame",
     {0xC0, 0x01, 0xDB, 0xC0, 0x02, 0xC0},
     6,
     "nnnEnF",
     1,
     {{1, {0x02}}}},

    {"ESC ESC is invalid", {0xC0, 0xDB, 0xDB, 0xC0}, 4, "nnEn", 0, {{0, {0}}}},

    {"data after an error is discarded until END",
     {0xC0, 0xDB, 0x00, 0x01, 0x02, 0xC0, 0x03, 0xC0},
     8,
     "nnEnnnnF",
     1,
     {{1, {0x03}}}},

    {"lone ESC then END is an error", {0xDB, 0xC0}, 2, "nE", 0, {{0, {0}}}},

    {"no END yet: nothing is reported",
     {0x01, 0x02, 0x03},
     3,
     "nnn",
     0,
     {{0, {0}}}},
};

static void run_dec_cases(void) {
  for (size_t i = 0; i < sizeof dec_cases / sizeof dec_cases[0]; i++) {
    const dec_case_t *c = &dec_cases[i];
    if (strlen(c->results) != c->in_len) {
      printf("BAD TEST '%s': results length %zu != input length %zu\n", c->name,
             strlen(c->results), c->in_len);
      exit(2);
    }
    snprintf(ctx, sizeof ctx, "decode: %s", c->name);
    slip_decoder_t d;
    slip_decoder_init(&d);
    int ok = feed_and_compare(&d, c->in, c->in_len, c->results, c->fr, c->n_fr);
    printf("[%s] %s\n", ok ? "PASS" : "FAIL", c->name);
    tally(ok);
  }
}

/* Tiny builder for the long streams (overflow tests). */
typedef struct {
  uint8_t in[80];
  char res[81];
  size_t n;
} stream_t;

static void add(stream_t *s, uint8_t b, char r) {
  s->in[s->n] = b;
  s->res[s->n] = r;
  s->n++;
  s->res[s->n] = '\0';
}

static void run_long_cases(void) {
  frame_t fr[1];
  stream_t s;
  int ok;

  /* exactly SLIP_MAX_FRAME bytes is allowed */
  memset(&s, 0, sizeof s);
  add(&s, SLIP_END, 'n');
  for (size_t i = 0; i < SLIP_MAX_FRAME; i++) {
    add(&s, (uint8_t)(i + 1), 'n');
    fr[0].data[i] = (uint8_t)(i + 1);
  }
  add(&s, SLIP_END, 'F');
  fr[0].len = SLIP_MAX_FRAME;
  snprintf(ctx, sizeof ctx, "decode: frame of exactly 32 bytes");
  {
    slip_decoder_t d;
    slip_decoder_init(&d);
    ok = feed_and_compare(&d, s.in, s.n, s.res, fr, 1);
  }
  printf("[%s] frame of exactly %d bytes is accepted\n", ok ? "PASS" : "FAIL",
         SLIP_MAX_FRAME);
  tally(ok);

  /* 33 bytes: ERROR on the 33rd, then discard until END */
  memset(&s, 0, sizeof s);
  add(&s, SLIP_END, 'n');
  for (size_t i = 0; i < SLIP_MAX_FRAME; i++)
    add(&s, (uint8_t)(i + 1), 'n');
  add(&s, 0x55, 'E');
  add(&s, SLIP_END, 'n');
  snprintf(ctx, sizeof ctx, "decode: 33 bytes overflow");
  {
    slip_decoder_t d;
    slip_decoder_init(&d);
    ok = feed_and_compare(&d, s.in, s.n, s.res, NULL, 0);
  }
  printf("[%s] 33rd byte is an ERROR and the frame is discarded\n",
         ok ? "PASS" : "FAIL");
  tally(ok);

  /* overflow reached through an escape pair: ERROR on the second byte of the
   * pair */
  memset(&s, 0, sizeof s);
  add(&s, SLIP_END, 'n');
  for (size_t i = 0; i < SLIP_MAX_FRAME - 1; i++)
    add(&s, (uint8_t)(i + 1), 'n');
  add(&s, SLIP_ESC, 'n');
  add(&s, SLIP_ESC_END, 'n'); /* the 32nd byte, still fine */
  add(&s, SLIP_ESC, 'n');
  add(&s, SLIP_ESC_END, 'E'); /* the 33rd */
  add(&s, SLIP_END, 'n');
  snprintf(ctx, sizeof ctx, "decode: overflow through an escape pair");
  {
    slip_decoder_t d;
    slip_decoder_init(&d);
    ok = feed_and_compare(&d, s.in, s.n, s.res, NULL, 0);
  }
  printf("[%s] overflow through an escape pair reports ERROR on the pair's "
         "second byte\n",
         ok ? "PASS" : "FAIL");
  tally(ok);

  /* the decoder works again after an overflow */
  memset(&s, 0, sizeof s);
  add(&s, SLIP_END, 'n');
  for (size_t i = 0; i < SLIP_MAX_FRAME; i++)
    add(&s, (uint8_t)(i + 1), 'n');
  add(&s, 0x55, 'E');
  add(&s, SLIP_END, 'n');
  add(&s, 0x01, 'n');
  add(&s, SLIP_END, 'F');
  fr[0].len = 1;
  fr[0].data[0] = 0x01;
  snprintf(ctx, sizeof ctx, "decode: recovery after overflow");
  {
    slip_decoder_t d;
    slip_decoder_init(&d);
    ok = feed_and_compare(&d, s.in, s.n, s.res, fr, 1);
  }
  printf("[%s] decoder recovers after an overflow\n", ok ? "PASS" : "FAIL");
  tally(ok);
}

/* ---------- differential comparison against the reference decoder ---- */

static int compare_with_ref(const uint8_t *in, size_t n, size_t *frames_out) {
  int ok = 1;
  slip_decoder_t d;
  ref_dec_t r;
  slip_decoder_init(&d);
  ref_init(&r);
  size_t frames = 0;

  for (size_t i = 0; i < n; i++) {
    slip_result_t want = ref_feed(&r, in[i]);
    slip_result_t got = slip_decoder_feed(&d, in[i]);
    int bad = (got != want);
    if (!bad && want == SLIP_FRAME) {
      frames++;
      size_t len = slip_frame_len(&d);
      const uint8_t *data = slip_frame(&d);
      bad = (len != r.frame_len) ||
            (len > 0 && (data == NULL || memcmp(data, r.frame, len) != 0));
    }
    if (bad) {
      CHK(0, "byte %zu (0x%02X): got %s, expected %s (or frame mismatch)", i,
          in[i], char_name(res_char(got)), char_name(res_char(want)));
      if (fail_lines <= MAX_FAIL_LINES)
        printf("    stream so far: %s\n", hex(in, i + 1 > 40 ? 40 : i + 1));
      if (frames_out)
        *frames_out = frames;
      return 0;
    }
  }
  if (frames_out)
    *frames_out = frames;
  return ok;
}

static int test_random_stream(void) {
  uint8_t in[300];
  size_t n = 1 + (size_t)(rand() % 300);
  for (size_t i = 0; i < n; i++)
    in[i] = random_data_byte();
  snprintf(ctx, sizeof ctx, "random stream");
  return compare_with_ref(in, n, NULL);
}

/* Every byte value as a single data byte: encode, then decode, get the same
 * byte back. */
static int test_every_byte_roundtrip(unsigned b) {
  int ok = 1;
  snprintf(ctx, sizeof ctx, "round trip of the single data byte 0x%02X", b);
  uint8_t in = (uint8_t)b, out[4];
  size_t n = slip_encode(&in, 1, out, sizeof out);
  CHK(n == ((b == SLIP_END || b == SLIP_ESC) ? 4u : 3u), "encoded length %zu",
      n);
  if (n < 3 || n > 4)
    return 0;

  slip_decoder_t d;
  slip_decoder_init(&d);
  frame_t fr[1] = {{1, {in}}};
  char res[5];
  for (size_t i = 0; i + 1 < n; i++)
    res[i] = 'n';
  res[n - 1] = 'F';
  res[n] = '\0';
  CHK(feed_and_compare(&d, out, n, res, fr, 1), "decode failed");
  return ok;
}

/* Every byte value after ESC: DC and DD are valid, anything else is an error.
 */
static int test_every_byte_after_esc(unsigned b) {
  snprintf(ctx, sizeof ctx, "stream C0 DB %02X 01 C0", b);
  const uint8_t in[] = {SLIP_END, SLIP_ESC, (uint8_t)b, 0x01, SLIP_END};
  return compare_with_ref(in, sizeof in, NULL);
}

/* Encode random payloads (1..32 bytes), feed them through the decoder, get them
 * back. */
static int test_roundtrip_single(void) {
  int ok = 1;
  size_t n = 1 + (size_t)(rand() % SLIP_MAX_FRAME);
  uint8_t payload[SLIP_MAX_FRAME], enc[2 * SLIP_MAX_FRAME + 2];
  for (size_t i = 0; i < n; i++)
    payload[i] = random_data_byte();

  snprintf(ctx, sizeof ctx, "round trip, payload len=%zu", n);
  size_t m = slip_encode(payload, n, enc, sizeof enc);
  CHK(m >= n + 2, "encoder returned %zu", m);
  if (m < n + 2)
    return 0;

  slip_decoder_t d;
  slip_decoder_init(&d);
  frame_t fr[1];
  fr[0].len = n;
  memcpy(fr[0].data, payload, n);
  char res[2 * SLIP_MAX_FRAME + 3];
  for (size_t i = 0; i + 1 < m; i++)
    res[i] = 'n';
  res[m - 1] = 'F';
  res[m] = '\0';
  CHK(feed_and_compare(&d, enc, m, res, fr, 1), "decode failed");
  return ok;
}

/* Several frames in one stream, sometimes sharing an END. */
static int test_roundtrip_multi(void) {
  int ok = 1;
  uint8_t stream[3 * (2 * 10 + 2)];
  size_t total = 0;
  frame_t fr[3];

  for (int f = 0; f < 3; f++) {
    size_t n = 1 + (size_t)(rand() % 10);
    uint8_t payload[10], enc[22];
    for (size_t i = 0; i < n; i++)
      payload[i] = random_data_byte();
    size_t m = slip_encode(payload, n, enc, sizeof enc);
    snprintf(ctx, sizeof ctx, "multi-frame round trip");
    CHK(m >= n + 2, "encoder returned %zu", m);
    if (m < n + 2)
      return 0;
    size_t skip =
        (f > 0 && (rand() % 2)) ? 1 : 0; /* share the previous frame's END */
    memcpy(stream + total, enc + skip, m - skip);
    total += m - skip;
    fr[f].len = n;
    memcpy(fr[f].data, payload, n);
  }

  snprintf(ctx, sizeof ctx, "multi-frame round trip (3 frames)");
  size_t frames = 0;
  CHK(compare_with_ref(stream, total, &frames),
      "differs from the reference decoder");
  CHK(frames == 3, "decoded %zu frames, expected 3", frames);

  slip_decoder_t d;
  slip_decoder_init(&d);
  size_t seen = 0;
  for (size_t i = 0; i < total; i++) {
    slip_result_t r = slip_decoder_feed(&d, stream[i]);
    if (r == SLIP_FRAME && seen < 3) {
      size_t len = slip_frame_len(&d);
      CHK(len == fr[seen].len &&
              memcmp(slip_frame(&d), fr[seen].data, len) == 0,
          "frame %zu does not match the payload that was encoded", seen);
      seen++;
    }
  }
  return ok;
}

/* ------------------- independence and reset tests ------------------- */

static int test_independent(void) {
  int ok = 1;
  snprintf(ctx, sizeof ctx, "two decoders interleaved");

  const uint8_t sa[] = {SLIP_END,     0x01, SLIP_ESC,
                        SLIP_ESC_END, 0x02, SLIP_END}; /* frame [01 C0 02] */
  const char *ra = "nnnnnF";
  frame_t fa[1] = {{3, {0x01, 0xC0, 0x02}}};

  const uint8_t sb[] = {SLIP_END, SLIP_ESC, SLIP_ESC_ESC,
                        SLIP_END}; /* frame [DB] */
  const char *rb = "nnnF";
  frame_t fb[1] = {{1, {0xDB}}};

  slip_decoder_t a, b;
  slip_decoder_init(&a);
  slip_decoder_init(&b);
  size_t seen_a = 0, seen_b = 0;

  for (size_t i = 0; i < sizeof sa; i++) {
    if (!check_byte(&a, sa[i], i, ra[i], fa, 1, &seen_a))
      ok = 0;
    if (i < sizeof sb)
      if (!check_byte(&b, sb[i], i, rb[i], fb, 1, &seen_b))
        ok = 0;
  }
  CHK(seen_a == 1 && seen_b == 1, "frames seen: A=%zu B=%zu, expected 1 each",
      seen_a, seen_b);
  return ok;
}

/* slip_decoder_init in the middle of an escape must forget the escape. */
static int test_reset_mid_escape(void) {
  int ok = 1;
  snprintf(ctx, sizeof ctx, "slip_decoder_init while an ESC is pending");

  slip_decoder_t d;
  slip_decoder_init(&d);
  const uint8_t part[] = {SLIP_END, 0x01, SLIP_ESC};
  CHK(feed_and_compare(&d, part, sizeof part, "nnn", NULL, 0),
      "partial stream failed");

  slip_decoder_init(&d);
  /* DC is plain data now, not an escaped END */
  const uint8_t again[] = {SLIP_ESC_END, SLIP_END};
  frame_t fr[1] = {{1, {SLIP_ESC_END}}};
  CHK(feed_and_compare(&d, again, sizeof again, "nF", fr, 1),
      "stale escape state survived init");
  return ok;
}

/* slip_decoder_init while discarding must stop discarding. */
static int test_reset_while_discarding(void) {
  int ok = 1;
  snprintf(ctx, sizeof ctx,
           "slip_decoder_init while discarding after an error");

  slip_decoder_t d;
  slip_decoder_init(&d);
  const uint8_t bad[] = {SLIP_END, SLIP_ESC, 0x01};
  CHK(feed_and_compare(&d, bad, sizeof bad, "nnE", NULL, 0),
      "error stream failed");

  slip_decoder_init(&d);
  const uint8_t again[] = {0x02, SLIP_END};
  frame_t fr[1] = {{1, {0x02}}};
  CHK(feed_and_compare(&d, again, sizeof again, "nF", fr, 1),
      "still discarding after init");
  return ok;
}

/* ------------------------------ main ------------------------------- */

int main(void) {
  int before;

  printf("--- encoder ---\n");
  for (size_t i = 0; i < sizeof enc_cases / sizeof enc_cases[0]; i++) {
    int ok = test_encode_case(&enc_cases[i]);
    printf("[%s] %s\n", ok ? "PASS" : "FAIL", enc_cases[i].name);
    tally(ok);
  }
  before = tests_failed;
  srand(4242);
  for (int i = 0; i < 5000; i++)
    tally(test_encode_random());
  printf("[%s] 5000 random inputs vs. reference (and END appears only at both "
         "ends)\n",
         tests_failed == before ? "PASS" : "FAIL");

  printf("\n--- decoder examples ---\n");
  run_dec_cases();
  run_long_cases();

  printf("\n--- sweeps and round trips ---\n");
  before = tests_failed;
  for (unsigned b = 0; b <= 255; b++)
    tally(test_every_byte_roundtrip(b));
  printf("[%s] every byte value survives encode then decode (256 cases)\n",
         tests_failed == before ? "PASS" : "FAIL");

  before = tests_failed;
  for (unsigned b = 0; b <= 255; b++)
    tally(test_every_byte_after_esc(b));
  printf("[%s] every byte value after ESC matches the reference (256 cases)\n",
         tests_failed == before ? "PASS" : "FAIL");

  before = tests_failed;
  for (int i = 0; i < 2000; i++)
    tally(test_roundtrip_single());
  printf("[%s] 2000 random payloads, 1..32 bytes, round trip\n",
         tests_failed == before ? "PASS" : "FAIL");

  before = tests_failed;
  for (int i = 0; i < 2000; i++)
    tally(test_roundtrip_multi());
  printf("[%s] 2000 three-frame streams, some sharing an END\n",
         tests_failed == before ? "PASS" : "FAIL");

  printf("\n--- state handling ---\n");
  int ok_a = test_independent();
  tally(ok_a);
  printf("[%s] two decoders do not share state\n", ok_a ? "PASS" : "FAIL");
  int ok_b = test_reset_mid_escape();
  tally(ok_b);
  printf("[%s] slip_decoder_init forgets a pending ESC\n",
         ok_b ? "PASS" : "FAIL");
  int ok_c = test_reset_while_discarding();
  tally(ok_c);
  printf("[%s] slip_decoder_init stops discarding\n", ok_c ? "PASS" : "FAIL");

  printf("\n--- random streams vs. reference decoder ---\n");
  before = tests_failed;
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
