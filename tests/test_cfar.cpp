#include <cmath>
#include <random>

#include "cfar.h"
#include "pulse_compression.h"
#include "range_doppler.h"
#include "signal_gen.h"
#include "test_util.h"

using namespace radar;

namespace {
RMatrix exponential_noise_map(std::size_t rows, std::size_t cols, std::mt19937_64& rng) {
  std::exponential_distribution<double> e(1.0);  // |karmaşık Gauss|^2, ortalama 1
  RMatrix m(rows, cols);
  for (double& v : m.data()) v = e(rng);
  return m;
}

bool detected_at(const std::vector<Detection>& d, std::size_t dop, std::size_t rng) {
  for (const Detection& x : d)
    if (x.doppler_bin == dop && x.range_bin == rng) return true;
  return false;
}
}  // namespace

int main() {
  const RadarParams p;
  CfarParams cfar;
  const std::size_t n = cfar_num_training_cells(cfar);

  // 1) Eşik çarpanları: tasarım Pfa formüllerine geri yerleştirince Pfa çıkmalı.
  {
    CHECK(n == (2 * 8 + 1) * (2 * 4 + 1) - 5 * 3);
    const double a = ca_cfar_alpha(n, 1e-6);
    CHECK_NEAR(std::pow(1.0 + a / static_cast<double>(n), -static_cast<double>(n)), 1e-6, 1e-9);
    const std::size_t k = static_cast<std::size_t>(0.75 * static_cast<double>(n));
    const double t = os_cfar_alpha(n, k, 1e-6);
    double pfa = 1.0;
    for (std::size_t i = 0; i < k; ++i)
      pfa *= static_cast<double>(n - i) / (static_cast<double>(n - i) + t);
    CHECK_NEAR(std::log10(pfa), -6.0, 1e-6);
  }

  // 2) Yalnızca gürültü: gerçekleşen yanlış alarm oranı tasarım Pfa'sına yakın olmalı (CA ve OS).
  {
    std::mt19937_64 rng(21);
    cfar.pfa = 1e-2;
    cfar.local_maxima_only = false;
    const std::size_t rows = 64, cols = 512, wr = cfar.guard_range + cfar.train_range;
    const double tested = static_cast<double>(rows * (cols - 2 * wr));
    for (CfarType type : {CfarType::CellAveraging, CfarType::OrderedStatistic}) {
      std::size_t hits = 0;
      const int frames = 10;
      for (int f = 0; f < frames; ++f)
        hits += cfar_detect(exponential_noise_map(rows, cols, rng), p, cfar, type).size();
      const double measured = static_cast<double>(hits) / (frames * tested);
      CHECK_NEAR(measured / 1e-2, 1.0, 0.2);  // tasarımın %20'si içinde
    }
  }

  // 3) Maskeleme: eğitim penceresindeki çok güçlü komşu CA-CFAR'ı kör eder, OS-CFAR etmez.
  {
    std::mt19937_64 rng(5);
    cfar.pfa = 1e-6;
    cfar.local_maxima_only = false;
    RMatrix map = exponential_noise_map(64, 512, rng);
    map(32, 200) = 316.0;    // 25 dB hedef
    map(32, 205) = 10000.0;  // 40 dB girişimci, hedefin eğitim penceresinde
    const auto ca = cfar_detect(map, p, cfar, CfarType::CellAveraging);
    const auto os = cfar_detect(map, p, cfar, CfarType::OrderedStatistic);
    CHECK(!detected_at(ca, 32, 200));
    CHECK(detected_at(os, 32, 200));
  }

  // 4) Uçtan uca: gürültülü ham veriden hedefin menzil/hızı doğru çıkar.
  {
    std::mt19937_64 rng(99);
    cfar.pfa = 1e-6;
    cfar.local_maxima_only = true;
    CMatrix frame(p.num_pulses, p.num_samples);
    Target t;
    t.range_m = 200.0 * p.range_bin_m();
    t.velocity_mps = 7.0 * p.velocity_resolution_mps();
    t.amplitude = amplitude_for_integrated_snr(p, 25.0, 1.0);
    add_target(frame, p, t);
    add_noise(frame, 1.0, rng);
    const PulseCompressor pc(make_lfm_chirp(p), p.num_samples);
    const RMatrix rd = range_doppler_power(pc.compress(frame));
    for (CfarType type : {CfarType::CellAveraging, CfarType::OrderedStatistic}) {
      const auto det = cfar_detect(rd, p, cfar, type);
      CHECK(!det.empty());
      bool found = false;
      for (const Detection& d : det)
        if (std::abs(d.range_m - t.range_m) <= p.range_bin_m() &&
            std::abs(d.velocity_mps - t.velocity_mps) <= p.velocity_resolution_mps())
          found = true;
      CHECK(found);
      CHECK(det.size() <= 3);  // tek hedef -> birkaç adetten fazla sahte tespit olmamalı
    }
  }

  return testing::test_summary("test_cfar");
}
