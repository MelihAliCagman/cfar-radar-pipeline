// main.cpp — radar_demo: simüle radar hattını uçtan uca çalıştırır.
#include <cstdio>

#include "radar_params.h"
#include "signal_gen.h"

int main() {
  using namespace radar;
  const RadarParams p;

  std::printf("=== cfar-radar-pipeline ===\n");
  std::printf("Dalga boyu            : %.4f m\n", p.wavelength());
  std::printf("Menzil cozunurlugu    : %.2f m  (c / 2B)\n", p.range_resolution_m());
  std::printf("Menzil hucresi        : %.2f m  (c / 2fs)\n", p.range_bin_m());
  std::printf("Maks. menzil (pencere): %.0f m\n",
              p.range_bin_m() * static_cast<double>(p.num_samples));
  std::printf("Hiz cozunurlugu       : %.2f m/s\n", p.velocity_resolution_mps());
  std::printf("Belirsiz hiz siniri   : +-%.1f m/s\n", p.max_unambiguous_velocity_mps());
  return 0;
}
