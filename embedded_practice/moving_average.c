/*
 * Test script for a fixed-window moving average over byte samples.
 *
 * Build and run (default window N = 8):
 *   gcc -std=c11 -Wall -Wextra -g -O0 -fsanitize=address,undefined
 * test_moving_avg.c -o test_moving_avg && ./test_moving_avg
 *
 * The window size is a compile-time constant. Try several values, because edge
 * cases move with N: for n in 1 2 3 7 8 16 64 255; do gcc -std=c11 -Wall
 * -Wextra -g -O0 -fsanitize=address,undefined -DMA_WINDOW=$n test_moving_avg.c
 * -o t && ./t | tail -1 done
 *
 * Problem:
 *   Samples (bytes, 0..255) arrive one at a time, for example ADC readings.
 * After each sample, report the average of the most recent samples, using a
 * fixed amount of memory (no malloc) and constant work per sample.
 *
 *     void    ma_init(ma_t *m);                   an empty window.
 *     uint8_t ma_add(ma_t *m, uint8_t sample);    add a sample, return the new
 * average. uint8_t ma_average(const ma_t *m);          the current average
 * without adding anything. size_t  ma_count(const ma_t *m);            how many
 * samples are in the window right now (0..N).
 *
 *   Rules:
 *     1. The window holds the most recent N samples (N = MA_WINDOW). Once it is
 * full, each new sample replaces the oldest one.
 *     2. Warm-up: until N samples have arrived, the average is over the samples
 * received so far, so the first sample's average is the sample itself. (Do not
 * divide by N too early.)
 *     3. The average is truncated: sum / count using integer division, not
 * rounded.
 *     4. With no samples yet, ma_average returns 0 and ma_count returns 0.
 *     5. ma_init fully resets the window, including a window that has been used
 * before.
 *     6. No global state: two ma_t instances must work independently.
 *
 * Example with N = 4:
 *     add 10 -> 10    (1 sample: 10 / 1)
 *     add 20 -> 15    (2 samples: 30 / 2)
 *     add 31 -> 20    (3 samples: 61 / 3 = 20, truncated)
 *     add 40 -> 25    (4 samples: 101 / 4 = 25, truncated)
 *     add 50 -> 35    (oldest 10 dropped: 20 + 31 + 40 + 50 = 141 / 4 = 35,
 * truncated)
 *
 * What the tests do:
 *   - named streams, each checked at every step against a brute-force reference
 * (it re-sums the last N samples from scratch each time)
 *   - every stream of length 9 over the values {0, 128, 255}, plus long random
 * streams that wrap many times
 *   - ma_average and ma_count checked after every sample
 *   - re-initialization and two instances interleaved
 *   - the sum must be wider than a byte: N samples of 255 add up to 255 * N
 *
 * Replace the stub section with your implementation (or #include your own file
 * instead).
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MA_WINDOW
#define MA_WINDOW 8
#endif

/* ------------------------------------------------------------------ */
/* TODO: implement these (or #include your own file instead).          */
/* Design ma_t yourself.                                               */
/* ------------------------------------------------------------------ */

typedef struct {
  // Ring buffer included
  int head;
  int tail;
  int data[MA_WINDOW];
  size_t size;
  // Keep track of latest sum
  int running_sum;
} ma_t;

void ma_init(ma_t *m) {
  m->head = 0;
  m->tail = -1;
  m->size = 0;
  m->running_sum = 0;
}

uint8_t ma_average(const ma_t *m) {
  if (m->size == 0) {
    return 0;
  }
  return m->running_sum / m->size; 
}

size_t ma_count(const ma_t *m) { 
  return m->size; 
}


// Drop the oldest value
// which is the one at the front (head) of the queue 
// and return it
int ma_drop(ma_t *m) {
  if (m->size == 0) {
    return -1;
  }

  int oldest = m->data[m->head];
  m->head = (m->head + 1) % MA_WINDOW;
  m->size--;
  return oldest;
}

uint8_t ma_add(ma_t *m, uint8_t sample) {
  if (m->size == MA_WINDOW) { // full, drop the oldest one and remove from sum
    int oldest = ma_drop(m);
    m->running_sum -= oldest;
  }

  // Add the new one to the buffer and update the running sum
  m->tail = (m->tail + 1) % MA_WINDOW;
  m->data[m->tail] = sample;
  m->size++;
  m->running_sum += sample;

  return ma_average(m);
}

/* ------------------------------------------------------------------ */
/* Test harness                                                        */
/* ------------------------------------------------------------------ */

#define MAX_FAIL_LINES 40
#define MAX_HISTORY 4096

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

/* Brute-force reference: re-sums the last N samples from scratch. */
typedef struct {
  uint8_t hist[MAX_HISTORY];
  size_t n;
} ref_t;

static void ref_init(ref_t *r) { r->n = 0; }

static size_t ref_count(const ref_t *r) {
  return r->n < MA_WINDOW ? r->n : (size_t)MA_WINDOW;
}

static uint8_t ref_avg(const ref_t *r) {
  size_t c = ref_count(r);
  if (c == 0)
    return 0;
  unsigned long sum = 0;
  for (size_t i = r->n - c; i < r->n; i++)
    sum += r->hist[i];
  return (uint8_t)(sum / c);
}

static uint8_t ref_add(ref_t *r, uint8_t s) {
  r->hist[r->n++] = s;
  return ref_avg(r);
}

/* Feed a stream and compare every step: the value returned by ma_add, then
 * ma_average and ma_count. */
static int check_stream(const uint8_t *s, size_t len) {
  int ok = 1;
  ma_t m;
  ref_t r;
  ma_init(&m);
  ref_init(&r);

  CHK(ma_count(&m) == 0, "after init: count = %zu, expected 0", ma_count(&m));
  CHK(ma_average(&m) == 0, "after init: average = %u, expected 0",
      ma_average(&m));

  for (size_t i = 0; i < len; i++) {
    uint8_t want = ref_add(&r, s[i]);
    uint8_t got = ma_add(&m, s[i]);
    CHK(got == want, "sample %zu (value %u): ma_add returned %u, expected %u",
        i, s[i], got, want);
    CHK(ma_average(&m) == want, "sample %zu: ma_average = %u, expected %u", i,
        ma_average(&m), want);
    CHK(ma_count(&m) == ref_count(&r),
        "sample %zu: ma_count = %zu, expected %zu", i, ma_count(&m),
        ref_count(&r));
    if (!ok)
      return 0; /* the first wrong step is the useful one */
  }
  return ok;
}

/* ----------------------------- named streams ----------------------- */

typedef struct {
  const char *name;
  uint8_t s[32];
  size_t len;
} named_t;

static const named_t named[] = {
    {"a single sample", {10}, 1},
    {"two samples (warm-up: divide by 2)", {10, 20}, 2},
    {"truncation, not rounding", {1, 2}, 2},
    {"truncation on three samples", {10, 20, 31}, 3},
    {"the example from the problem", {10, 20, 31, 40, 50}, 5},
    {"constant stream (warm-up must not dip)",
     {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
     12},
    {"a lone 255", {255}, 1},
    {"a spike after zeros", {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255}, 11},
    {"an old spike leaving the window",
     {255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
     17},
    {"ascending ramp",
     {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20},
     20},
    {"alternating 0 and 255",
     {0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255},
     16},
};

static void run_named(void) {
  for (size_t i = 0; i < sizeof named / sizeof named[0]; i++) {
    snprintf(ctx, sizeof ctx, "%s", named[i].name);
    int ok = check_stream(named[i].s, named[i].len);
    printf("[%s] %s\n", ok ? "PASS" : "FAIL", named[i].name);
    tally(ok);
  }

  /* A long run of 255s: the sum reaches 255 * N, which does not fit in a byte.
   */
  {
    uint8_t s[3 * 300];
    size_t len = (size_t)(3 * MA_WINDOW) + 3;
    if (len > sizeof s)
      len = sizeof s;
    memset(s, 255, len);
    snprintf(ctx, sizeof ctx,
             "all 255 for 3N+3 samples (largest possible sum)");
    int ok = check_stream(s, len);
    printf("[%s] all 255 for 3N+3 samples: the sum reaches 255 * N = %u\n",
           ok ? "PASS" : "FAIL", 255u * MA_WINDOW);
    tally(ok);
  }
}

/* ----------------------------- exhaustive -------------------------- */

static void run_exhaustive(void) {
  static const uint8_t alphabet[3] = {0, 128, 255};
  const size_t LEN = 9;
  int before = tests_failed;
  size_t total = 1;
  for (size_t i = 0; i < LEN; i++)
    total *= 3;

  for (size_t code = 0; code < total; code++) {
    uint8_t s[9];
    size_t c = code;
    for (size_t i = 0; i < LEN; i++) {
      s[i] = alphabet[c % 3];
      c /= 3;
    }
    snprintf(ctx, sizeof ctx, "exhaustive stream #%zu", code);
    tally(check_stream(s, LEN));
  }
  printf("[%s] every stream of length 9 over {0, 128, 255} (%zu streams)\n",
         tests_failed == before ? "PASS" : "FAIL", total);
}

/* ----------------------------- state handling ---------------------- */

static int test_reinit(void) {
  int ok = 1;
  snprintf(ctx, sizeof ctx, "ma_init on a window that has been used");
  ma_t m;
  ma_init(&m);
  for (int i = 0; i < 3 * MA_WINDOW + 1; i++)
    ma_add(&m, 200);

  ma_init(&m);
  CHK(ma_count(&m) == 0, "after re-init: count = %zu, expected 0",
      ma_count(&m));
  CHK(ma_average(&m) == 0, "after re-init: average = %u, expected 0",
      ma_average(&m));
  uint8_t got = ma_add(&m, 10);
  CHK(got == 10,
      "first sample after re-init: average = %u, expected 10 (old data leaked "
      "in)",
      got);
  got = ma_add(&m, 20);
  uint8_t want2 =
      (MA_WINDOW >= 2) ? 15 : 20; /* N = 1: only the latest sample counts */
  CHK(got == want2, "second sample after re-init: average = %u, expected %u",
      got, want2);
  return ok;
}

static int test_independent(void) {
  int ok = 1;
  snprintf(ctx, sizeof ctx, "two instances interleaved");
  ma_t a, b;
  ref_t ra, rb;
  ma_init(&a);
  ma_init(&b);
  ref_init(&ra);
  ref_init(&rb);
  srand(31337);
  for (int i = 0; i < 400; i++) {
    uint8_t va = (uint8_t)(rand() & 0xFF),
            vb = (uint8_t)(255 - (rand() & 0x3F));
    uint8_t wa = ref_add(&ra, va), wb = ref_add(&rb, vb);
    uint8_t ga = ma_add(&a, va), gb = ma_add(&b, vb);
    CHK(ga == wa, "instance A, step %d: got %u, expected %u", i, ga, wa);
    CHK(gb == wb, "instance B, step %d: got %u, expected %u", i, gb, wb);
    if (!ok)
      return 0;
  }
  return ok;
}

/* ----------------------------- random ------------------------------ */

static uint8_t rnd_sample(void) {
  int r = rand() % 10;
  if (r == 0)
    return 0;
  if (r == 1)
    return 255;
  if (r == 2)
    return (uint8_t)(250 + rand() % 6);
  return (uint8_t)(rand() & 0xFF);
}

static void run_random(void) {
  int before = tests_failed;
  srand(60601);
  const int ITER = 3000;
  for (int i = 0; i < ITER; i++) {
    uint8_t s[MAX_HISTORY];
    size_t len = 1 + (size_t)(rand() % (10 * MA_WINDOW + 50));
    if (len > MAX_HISTORY)
      len = MAX_HISTORY;
    for (size_t k = 0; k < len; k++)
      s[k] = rnd_sample();
    snprintf(ctx, sizeof ctx, "random stream #%d, len=%zu", i, len);
    tally(check_stream(s, len));
  }
  printf("[%s] %d random streams, including ones that wrap the window many "
         "times\n",
         tests_failed == before ? "PASS" : "FAIL", ITER);
}

/* ------------------------------ main ------------------------------- */

int main(void) {
  printf("window size N = %d\n\n", MA_WINDOW);

  printf("--- named streams ---\n");
  run_named();

  printf("\n--- exhaustive and random ---\n");
  run_exhaustive();
  run_random();

  printf("\n--- state handling ---\n");
  int ok_a = test_reinit();
  tally(ok_a);
  printf("[%s] ma_init fully resets a used window\n", ok_a ? "PASS" : "FAIL");
  int ok_b = test_independent();
  tally(ok_b);
  printf("[%s] two instances do not share state\n", ok_b ? "PASS" : "FAIL");

  if (fail_lines > MAX_FAIL_LINES)
    printf("\n(%d further failure lines suppressed)\n",
           fail_lines - MAX_FAIL_LINES);

  printf("\n%d/%d cases passed\n", tests_run - tests_failed, tests_run);
  return tests_failed != 0;
}
