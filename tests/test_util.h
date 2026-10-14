// test_util.h — Dış bağımlılık olmadan minimal test çerçevesi (CHECK makroları).
// Bir CHECK başarısız olursa mesaj yazılır, test devam eder; sonunda test_summary()
// sıfır olmayan çıkış kodu döndürür ve CTest bunu "FAILED" olarak raporlar.
#pragma once

#include <cmath>
#include <cstdio>

namespace testing {
inline int& failures() {
  static int count = 0;
  return count;
}
inline int test_summary(const char* name) {
  if (failures() == 0) {
    std::printf("[ OK ] %s\n", name);
    return 0;
  }
  std::printf("[FAIL] %s: %d check(s) failed\n", name, failures());
  return 1;
}
}  // namespace testing

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::printf("  CHECK failed: %s  (%s:%d)\n", #cond, __FILE__, __LINE__); \
      ++testing::failures();                                                   \
    }                                                                          \
  } while (0)

#define CHECK_NEAR(a, b, tol)                                                          \
  do {                                                                                 \
    const double va_ = (a), vb_ = (b);                                                 \
    if (!(std::fabs(va_ - vb_) <= (tol))) {                                            \
      std::printf("  CHECK_NEAR failed: %s=%g vs %s=%g (tol %g)  (%s:%d)\n", #a, va_, \
                  #b, vb_, static_cast<double>(tol), __FILE__, __LINE__);              \
      ++testing::failures();                                                           \
    }                                                                                  \
  } while (0)
