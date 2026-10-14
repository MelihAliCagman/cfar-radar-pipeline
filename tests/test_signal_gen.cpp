#include <cmath>

#include "signal_gen.h"
#include "test_util.h"

using namespace radar;

int main() {
  const RadarParams p;

  // Türetilen büyüklükleri elle hesapla: c/(2B)=14.99 m, c/(2fs)=7.495 m.
  CHECK_NEAR(p.range_resolution_m(), 14.9896, 1e-3);
  CHECK_NEAR(p.range_bin_m(), 7.4948, 1e-3);
  CHECK(p.chirp_samples() == 200);
  CHECK_NEAR(p.velocity_resolution_mps(), 0.03 * 10e3 / (2 * 64), 0.01);

  // Chirp: her örnekte birim genlik (sabit zarf).
  const Chirp chirp = make_lfm_chirp(p);
  CHECK(chirp.size() == 200);
  for (const Complex& x : chirp) CHECK_NEAR(std::abs(x), 1.0, 1e-12);

  // Hedef yankısı: sıfır olmayan örnekler gecikmeden sonraki ilk tam örnekte başlamalı ve
  // chirp süresi (L örnek) kadar sürmeli. 30.25 örnek gecikme = kesirli gecikme testi.
  {
    CMatrix frame(p.num_pulses, p.num_samples);
    Target t;
    t.range_m = 30.25 * p.range_bin_m();  // gecikme = 30.25 örnek
    add_target(frame, p, t);
    std::size_t first = 0, last = 0;
    bool seen = false;
    for (std::size_t n = 0; n < frame.cols(); ++n) {
      if (std::abs(frame(0, n)) > 0) {
        if (!seen) first = n;
        seen = true;
        last = n;
      }
    }
    CHECK(first == 31);                         // ceil(30.25)
    CHECK(last == 30 + p.chirp_samples());      // n < 30.25 + 200  ->  n <= 230
    CHECK_NEAR(std::abs(frame(0, 100)), 1.0, 1e-9);
  }

  // Doppler: darbeden darbeye faz dönüşü 2*pi*fd/PRF olmalı (fd = 2v/lambda).
  {
    CMatrix frame(p.num_pulses, p.num_samples);
    Target t;
    t.range_m = 30.0 * p.range_bin_m();
    t.velocity_mps = 20.0;
    add_target(frame, p, t);
    const double expected = 2.0 * kPi * (2.0 * 20.0 / p.wavelength()) / p.prf_hz;
    const double measured = std::arg(frame(1, 100) / frame(0, 100));
    CHECK_NEAR(std::remainder(measured - expected, 2.0 * kPi), 0.0, 1e-6);
  }

  // Gürültü gücü: örnek ortalaması istenen E[|n|^2] değerine yakın olmalı.
  {
    CMatrix frame(p.num_pulses, p.num_samples);
    std::mt19937_64 rng(1);
    add_noise(frame, 4.0, rng);
    double power = 0.0;
    for (const Complex& x : frame.data()) power += std::norm(x);
    power /= static_cast<double>(frame.data().size());
    CHECK_NEAR(power, 4.0, 0.1);
  }

  // SNR yardımcısı: genlik^2 x toplam kazanç = SNR x gürültü gücü.
  {
    const double a = amplitude_for_integrated_snr(p, 20.0, 1.0);
    CHECK_NEAR(a * a * p.compression_gain() * static_cast<double>(p.num_pulses), 100.0, 1e-6);
  }

  return testing::test_summary("test_signal_gen");
}
