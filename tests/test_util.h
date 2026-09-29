#pragma once

#include <cstdio>

inline int g_failures = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++g_failures;                                                        \
    }                                                                      \
  } while (0)

inline int Finish(const char* name) {
  if (g_failures == 0) std::printf("all %s tests passed\n", name);
  return g_failures == 0 ? 0 : 1;
}
