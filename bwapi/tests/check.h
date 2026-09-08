#pragma once

// The whole test framework for the server-side validators.
//
// These suites exist because BWAPI's trusted side is Windows-only and untestable as a whole -
// Server.cpp reaches into BW memory through hardcoded offsets - while the input validation it
// performs is pure arithmetic that has to be right. Anything that can be pulled out of the
// server and answered without a game belongs here, where it runs on every push on any host.
//
// No dependency, on purpose: one header, a macro, and a nonzero exit code.

#include <cstdio>

namespace bwapi_test
{
  inline int failures = 0;
  inline int checks = 0;
}

#define CHECK(expr)                                                                    \
  do {                                                                                 \
    ++bwapi_test::checks;                                                              \
    if (!(expr)) {                                                                     \
      ++bwapi_test::failures;                                                          \
      std::fprintf(stderr, "%s:%d: FAILED: %s\n", __FILE__, __LINE__, #expr);          \
    }                                                                                  \
  } while (0)

#define CHECK_EQ(actual, expected)                                                     \
  do {                                                                                 \
    ++bwapi_test::checks;                                                              \
    const auto _a = (actual);                                                          \
    const auto _e = (expected);                                                        \
    if (!(_a == _e)) {                                                                 \
      ++bwapi_test::failures;                                                          \
      std::fprintf(stderr, "%s:%d: FAILED: %s == %s (got %lld, want %lld)\n",          \
                   __FILE__, __LINE__, #actual, #expected,                             \
                   static_cast<long long>(_a), static_cast<long long>(_e));            \
    }                                                                                  \
  } while (0)

#define TEST_MAIN_EPILOGUE()                                                           \
  do {                                                                                 \
    std::fprintf(stderr, "%d checks, %d failed\n", bwapi_test::checks,                 \
                 bwapi_test::failures);                                                \
    return bwapi_test::failures == 0 ? 0 : 1;                                          \
  } while (0)
