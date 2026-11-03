/*
 * Test script for wraparound-safe time comparison.
 *
 * Build and run:
 *   gcc -std=c11 -Wall -Wextra -g -O0 -fsanitize=address,undefined
 * test_time_wrap.c -o test_time_wrap && ./test_time_wrap
 *
 * Problem:
 *   A 32-bit millisecond tick counter is read from a timer. It counts up and
 * wraps from 0xFFFFFFFF back to 0 after about 49.7 days. Write helpers that
 * compare times correctly even when the counter wraps.
 *
 *   Two times are always less than half the counter range apart (under 2^31 ms,
 * about 24.8 days), so for any two readings a and b, a is "later than" b if a
 * is ahead of b by a distance between 1 and 0x7FFFFFFF (counting forward, with
 * wraparound).
 *
 *     int      time_after(uint32_t a, uint32_t b);      1 if a is later than b,
 * else 0. int      time_before(uint32_t a, uint32_t b);     1 if a is earlier
 * than b, else 0. int      time_after_eq(uint32_t a, uint32_t b);   1 if a is
 * later than b or equal to b, else 0. uint32_t elapsed_ms(uint32_t now,
 * uint32_t start);   milliseconds from start to now, wrap-safe. int
 * timeout_expired(uint32_t now, uint32_t start, uint32_t timeout_ms); 1 if at
 * least timeout_ms have passed since start. timeout_ms is at most 0x7FFFFFFF.
 *
 *   When a is exactly 0x80000000 ahead of b, the order is ambiguous:
 * time_after, time_before, and time_after_eq all return 0 for that pair, in
 * both argument orders.
 *
 * Examples:
 *     time_after(100, 50)                    -> 1
 *     time_after(5, 0xFFFFFFFB)              -> 1   (the counter wrapped: 5 is
 * 10 ms AFTER 0xFFFFFFFB) time_before(0xFFFFFFFB, 5)             -> 1
 *     elapsed_ms(5, 0xFFFFFFFB)              -> 10
 *     timeout_expired(0x53, 0xFFFFFFF0, 100) -> 0   (99 ms have passed, one
 * short) timeout_expired(0x54, 0xFFFFFFF0, 100) -> 1
 *
 * What the tests do:
 *   - the named examples, including values on both sides of the wrap and the
 * 0x80000000 boundary
 *   - 200,000 random pairs generated from a 64-bit timeline that does NOT wrap,
 * truncated to 32 bits. The truth (which time is later, how much has elapsed)
 * is known from the 64-bit values, so the check does not depend on any formula.
 *   - properties: time_before(a, b) == time_after(b, a), and exactly one of
 * after, before, or equal holds
 *   - timeouts that begin just before the wrap
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
/* TODO: implement these (or #include your own file instead).          */
/* ------------------------------------------------------------------ */
static const uint32_t HALF_RANGE = 0x80000000;

int time_after(uint32_t a, uint32_t b) {
  if (a - b == HALF_RANGE || b - a == HALF_RANGE) {
    return 0;
  }

  if (a > b && a - b < HALF_RANGE) {
    return 1;
  } else if (a > b && a - b > HALF_RANGE) {
    // a has wrapped around
    return 0;
  }


  if (a < b && b - a > HALF_RANGE) {
    return 1;
  }
  else if (a < b && b - a < HALF_RANGE) {
    return 0;
  }

  return 0;
}

int time_before(uint32_t a, uint32_t b) { 
  return time_after(b, a);
}

int time_after_eq(uint32_t a, uint32_t b) {
  if (a == b) {
    return 1;
  }

  return time_after(a, b);
}

uint32_t elapsed_ms(uint32_t now, uint32_t start) { return now - start; }

int timeout_expired(uint32_t now, uint32_t start, uint32_t timeout_ms) {
  return elapsed_ms(now, start) >= timeout_ms;
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

/* ----------------------------- named examples ---------------------- */

typedef struct {
  const char *name;
  uint32_t a, b;
  int after, before, after_eq;
} order_case_t;

static const order_case_t order_cases[] = {
    {"100 is after 50", 100u, 50u, 1, 0, 1},
    {"50 is before 100", 50u, 100u, 0, 1, 0},
    {"equal times", 1234u, 1234u, 0, 0, 1},
    {"equal times at zero", 0u, 0u, 0, 0, 1},
    {"equal times at the maximum", 0xFFFFFFFFu, 0xFFFFFFFFu, 0, 0, 1},
    {"5 is 10 ms after 0xFFFFFFFB (wrapped)", 5u, 0xFFFFFFFBu, 1, 0, 1},
    {"0xFFFFFFFB is 10 ms before 5 (wrapped)", 0xFFFFFFFBu, 5u, 0, 1, 0},
    {"0 is 1 ms after 0xFFFFFFFF", 0u, 0xFFFFFFFFu, 1, 0, 1},
    {"0xFFFFFFFF is 1 ms before 0", 0xFFFFFFFFu, 0u, 0, 1, 0},
    {"1 ms ahead", 1001u, 1000u, 1, 0, 1},
    {"a is 0x7FFFFFFF ahead (the largest distance)", 0x7FFFFFFFu, 0u, 1, 0, 1},
    {"a is 0x7FFFFFFF behind", 0u, 0x7FFFFFFFu, 0, 1, 0},
    {"a is exactly 0x80000000 ahead: ambiguous", 0x80000000u, 0u, 0, 0, 0},
    {"a is exactly 0x80000000 behind: ambiguous", 0u, 0x80000000u, 0, 0, 0},
    {"a is 0x80000001 ahead, which is really behind", 0x80000001u, 0u, 0, 1, 0},
    {"a is 0x80000001 behind, which is really ahead", 0u, 0x80000001u, 1, 0, 1},
    {"large values, no wrap", 0xF0000010u, 0xF0000000u, 1, 0, 1},
};

static void run_order_cases(void) {
  for (size_t i = 0; i < sizeof order_cases / sizeof order_cases[0]; i++) {
    const order_case_t *c = &order_cases[i];
    int ok = 1;
    snprintf(ctx, sizeof ctx, "%s", c->name);
    int af = time_after(c->a, c->b), be = time_before(c->a, c->b),
        ae = time_after_eq(c->a, c->b);
    CHK(af == c->after, "time_after(0x%08X, 0x%08X) = %d, expected %d", c->a,
        c->b, af, c->after);
    CHK(be == c->before, "time_before(0x%08X, 0x%08X) = %d, expected %d", c->a,
        c->b, be, c->before);
    CHK(ae == c->after_eq, "time_after_eq(0x%08X, 0x%08X) = %d, expected %d",
        c->a, c->b, ae, c->after_eq);
    printf("[%s] %s\n", ok ? "PASS" : "FAIL", c->name);
    tally(ok);
  }
}

typedef struct {
  const char *name;
  uint32_t now, start, elapsed;
} elapsed_case_t;

static const elapsed_case_t elapsed_cases[] = {
    {"elapsed, no wrap", 100u, 50u, 50u},
    {"elapsed, same time", 777u, 777u, 0u},
    {"elapsed across the wrap", 5u, 0xFFFFFFFBu, 10u},
    {"elapsed across the wrap by 1", 0u, 0xFFFFFFFFu, 1u},
    {"elapsed, almost a full turn", 0xFFFFFFFFu, 0u, 0xFFFFFFFFu},
};

static void run_elapsed_cases(void) {
  for (size_t i = 0; i < sizeof elapsed_cases / sizeof elapsed_cases[0]; i++) {
    const elapsed_case_t *c = &elapsed_cases[i];
    int ok = 1;
    snprintf(ctx, sizeof ctx, "%s", c->name);
    uint32_t got = elapsed_ms(c->now, c->start);
    CHK(got == c->elapsed, "elapsed_ms(0x%08X, 0x%08X) = %u, expected %u",
        c->now, c->start, got, c->elapsed);
    printf("[%s] %s: %u\n", ok ? "PASS" : "FAIL", c->name, c->elapsed);
    tally(ok);
  }
}

typedef struct {
  const char *name;
  uint32_t now, start, timeout;
  int expired;
} timeout_case_t;

static const timeout_case_t timeout_cases[] = {
    {"not expired, no wrap", 1099u, 1000u, 100u, 0},
    {"expired exactly at the deadline", 1100u, 1000u, 100u, 1},
    {"expired after the deadline", 1500u, 1000u, 100u, 1},
    {"timeout of 0 expires immediately", 1000u, 1000u, 0u, 1},
    {"deadline wraps: one ms short", 0x53u, 0xFFFFFFF0u, 100u, 0},
    {"deadline wraps: exactly expired", 0x54u, 0xFFFFFFF0u, 100u, 1},
    {"deadline wraps: well past", 0x200u, 0xFFFFFFF0u, 100u, 1},
    {"just started, start near the wrap", 0xFFFFFFF0u, 0xFFFFFFF0u, 100u, 0},
    {"start + timeout lands exactly on 0", 0xFFFFFFFFu, 0xFFFFFF9Cu, 100u, 0},
    {"start + timeout lands exactly on 0, +1", 0u, 0xFFFFFF9Cu, 100u, 1},
    {"the longest timeout, one ms short", 0x7FFFFFFEu, 0u, 0x7FFFFFFFu, 0},
    {"the longest timeout, expired", 0x7FFFFFFFu, 0u, 0x7FFFFFFFu, 1},
};

static void run_timeout_cases(void) {
  for (size_t i = 0; i < sizeof timeout_cases / sizeof timeout_cases[0]; i++) {
    const timeout_case_t *c = &timeout_cases[i];
    int ok = 1;
    snprintf(ctx, sizeof ctx, "%s", c->name);
    int got = timeout_expired(c->now, c->start, c->timeout);
    CHK(got == c->expired,
        "timeout_expired(now=0x%08X, start=0x%08X, %u) = %d, expected %d",
        c->now, c->start, c->timeout, got, c->expired);
    printf("[%s] %s\n", ok ? "PASS" : "FAIL", c->name);
    tally(ok);
  }
}

/* ----------------------------- random timelines -------------------- */

static uint64_t rnd64(void) {
  return ((uint64_t)(rand() & 0xFFFF) << 48) |
         ((uint64_t)(rand() & 0xFFFF) << 32) |
         ((uint64_t)(rand() & 0xFFFF) << 16) | (uint64_t)(rand() & 0xFFFF);
}

/* A base time on a 64-bit timeline that does not wrap, with extra weight near
 * the 32-bit wrap points. */
static uint64_t pick_base(void) {
  uint64_t k = 1ULL << 32;
  switch (rand() % 6) {
  case 0:
    return k + (uint64_t)(rand() % 2000); /* just after a wrap */
  case 1:
    return 2 * k - 1000 + (uint64_t)(rand() % 2000); /* straddling a wrap */
  case 2:
    return k + (1ULL << 31) - 1000 +
           (uint64_t)(rand() % 2000); /* near the midpoint */
  case 3:
    return 2 * k + (uint64_t)(rand() % 2000);
  default:
    return k + (rnd64() % k); /* anywhere */
  }
}

/* A signed distance with magnitude below 2^31, weighted toward small values. */
static int64_t pick_delta(void) {
  int64_t mag;
  switch (rand() % 5) {
  case 0:
    mag = rand() % 4;
    break;
  case 1:
    mag = rand() % 1000;
    break;
  case 2:
    mag = (1LL << 31) - 1 - (rand() % 4);
    break; /* the edge of the allowed range */
  default:
    mag = (int64_t)(rnd64() % ((1ULL << 31) - 1ULL));
    break;
  }
  return (rand() & 1) ? mag : -mag;
}

static int test_random_pair(void) {
  int ok = 1;
  uint64_t t2 = pick_base();
  int64_t dt = pick_delta();
  uint64_t t1 = (uint64_t)((int64_t)t2 + dt);
  uint32_t a = (uint32_t)t1, b = (uint32_t)t2;

  snprintf(ctx, sizeof ctx,
           "a=0x%08X b=0x%08X (a is %lld ms from b on the real timeline)", a, b,
           (long long)dt);

  int want_after = dt > 0, want_before = dt < 0, want_eq = dt == 0;
  int af = time_after(a, b), be = time_before(a, b), ae = time_after_eq(a, b);
  CHK(af == want_after, "time_after = %d, expected %d", af, want_after);
  CHK(be == want_before, "time_before = %d, expected %d", be, want_before);
  CHK(ae == (want_after || want_eq), "time_after_eq = %d, expected %d", ae,
      want_after || want_eq);

  /* properties that must hold for any pair */
  CHK(time_before(b, a) == af,
      "time_before(b, a) should equal time_after(a, b)");
  CHK(af + be + (a == b) == 1,
      "exactly one of after, before, equal should hold (got %d + %d + %d)", af,
      be, a == b);

  /* elapsed and timeout: use whichever time is earlier as the start */
  uint64_t t_start = (dt >= 0) ? t2 : t1;
  uint64_t t_now = (dt >= 0) ? t1 : t2;
  uint32_t start = (uint32_t)t_start, now = (uint32_t)t_now;
  uint32_t want_elapsed = (uint32_t)(t_now - t_start);
  uint32_t got_elapsed = elapsed_ms(now, start);
  CHK(got_elapsed == want_elapsed,
      "elapsed_ms(0x%08X, 0x%08X) = %u, expected %u", now, start, got_elapsed,
      want_elapsed);

  uint32_t timeout = (uint32_t)(rnd64() % 0x80000000ULL);
  if (rand() % 3 == 0)
    timeout = want_elapsed; /* exactly at the deadline */
  if (rand() % 6 == 0 && want_elapsed > 0)
    timeout = want_elapsed + 1; /* one ms short */
  if (timeout > 0x7FFFFFFFu)
    timeout = 0x7FFFFFFFu;
  int want_exp = (uint64_t)want_elapsed >= (uint64_t)timeout;
  int got_exp = timeout_expired(now, start, timeout);
  CHK(got_exp == want_exp,
      "timeout_expired(now=0x%08X, start=0x%08X, %u) = %d, expected %d "
      "(elapsed %u)",
      now, start, timeout, got_exp, want_exp, want_elapsed);
  return ok;
}

/* ------------------------------ main ------------------------------- */

int main(void) {
  int before;

  printf("--- time_after, time_before, time_after_eq ---\n");
  run_order_cases();

  printf("\n--- elapsed_ms ---\n");
  run_elapsed_cases();

  printf("\n--- timeout_expired ---\n");
  run_timeout_cases();

  printf("\n--- random pairs from a 64-bit timeline ---\n");
  before = tests_failed;
  srand(424242);
  const int ITER = 200000;
  for (int i = 0; i < ITER; i++)
    tally(test_random_pair());
  printf(
      "[%s] %d random pairs (truth known from the non-wrapping 64-bit times)\n",
      tests_failed == before ? "PASS" : "FAIL", ITER);

  if (fail_lines > MAX_FAIL_LINES)
    printf("\n(%d further failure lines suppressed)\n",
           fail_lines - MAX_FAIL_LINES);

  printf("\n%d/%d cases passed\n", tests_run - tests_failed, tests_run);
  return tests_failed != 0;
}
