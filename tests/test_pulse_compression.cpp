#include <algorithm>
#include <cmath>

#include "pulse_compression.h"
#include "signal_gen.h"
#include "test_util.h"

using namespace radar;

namespace {
std::size_t peak_index(const CMatrix& m, std::size_t row) {
  std::size_t best = 0;
  for (std::size_t n = 1; n < m.cols(); ++n)
    if (std::abs(m(row, n)) > std::abs(m(row, best))) best = n;
  return best;
}
}  // namespace

int main() {
  const RadarParams p;
  const PulseCompressor pc(make_lfm_chirp(p), p.num_samples, WindowType::Rectangular);

  // 1) Tek hedef, gürültüsüz: tepe tam yankının başladığı örnekte, yüksekliği A * L.
  {
    CMatrix frame(p.num_pulses, p.num_samples);
    Target t;
    t.range_m = 100.0 * p.range_bin_m();  // gecikme = tam 100 örnek
    t.amplitude = 2.0;
    add_target(frame, p, t);
    const CMatrix out = pc.compress(frame);
    CHECK(peak_index(out, 0) == 100);
    CHECK_NEAR(std::abs(out(0, 100)), 2.0 * p.chirp_samples(), 1e-6);
  }

  // 2) Kesirli gecikme: tepe en yakın tam örneğe düşer (menzil hatası < 1 hücre).
  {
    CMatrix frame(p.num_pulses, p.num_samples);
    Target t;
    t.range_m = 150.4 * p.range_bin_m();
    add_target(frame, p, t);
    const std::size_t pk = peak_index(pc.compress(frame), 0);
    CHECK(pk == 150 || pk == 151);
  }

  // 3) Gürültü içinde: bütünleştirme öncesi SNR çok düşük (-5 dB) olsa da tepe bulunur,
  //    çünkü sıkıştırma L = 200 (~23 dB) kazanç sağlar.
  {
    CMatrix frame(p.num_pulses, p.num_samples);
    Target t;
    t.range_m = 220.0 * p.range_bin_m();
    t.amplitude = std::pow(10.0, -5.0 / 20.0);
    add_target(frame, p, t);
    std::mt19937_64 rng(11);
    add_noise(frame, 1.0, rng);
    int correct = 0;
    const CMatrix out = pc.compress(frame);
    for (std::size_t m = 0; m < frame.rows(); ++m)
      if (peak_index(out, m) == 220) ++correct;
    CHECK(correct >= 60);  // 64 darbenin neredeyse hepsinde
  }

  // 4) Çözünürlük: 2 hücre (~15 m = c/2B) arayla iki hedef ayrı tepe verir.
  {
    CMatrix frame(p.num_pulses, p.num_samples);
    Target a, b;
    a.range_m = 100.0 * p.range_bin_m();
    b.range_m = 104.0 * p.range_bin_m();  // 4 hücre = 30 m
    add_target(frame, p, a);
    add_target(frame, p, b);
    const CMatrix out = pc.compress(frame);
    CHECK(std::abs(out(0, 100)) > 0.8 * p.chirp_samples());
    CHECK(std::abs(out(0, 104)) > 0.8 * p.chirp_samples());
    CHECK(std::abs(out(0, 102)) < 0.3 * p.chirp_samples());  // aradaki çukur
  }

  // 5) Hamming ağırlığı menzil yan loblarını belirgin biçimde bastırır (tepeden >=8 hücre uzakta).
  {
    const PulseCompressor weighted(make_lfm_chirp(p), p.num_samples, WindowType::Hamming);
    CMatrix frame(p.num_pulses, p.num_samples);
    Target t;
    t.range_m = 100.0 * p.range_bin_m();
    add_target(frame, p, t);
    auto worst_sidelobe_db = [&](const PulseCompressor& c) {
      const CMatrix out = c.compress(frame);
      double peak = 0.0, side = 0.0;
      for (std::size_t n = 0; n < out.cols(); ++n) {
        const double a = std::abs(out(0, n));
        peak = std::max(peak, a);
        const long dist = std::labs(static_cast<long>(n) - 100);
        if (dist >= 8) side = std::max(side, a);
      }
      return 20.0 * std::log10(side / peak);
    };
    const double rect_db = worst_sidelobe_db(pc);
    const double hamm_db = worst_sidelobe_db(weighted);
    CHECK(hamm_db < rect_db - 10.0);
    CHECK(hamm_db < -30.0);
  }

  return testing::test_summary("test_pulse_compression");
}
