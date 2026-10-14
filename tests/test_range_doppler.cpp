#include <cmath>

#include "pulse_compression.h"
#include "range_doppler.h"
#include "signal_gen.h"
#include "test_util.h"

using namespace radar;

namespace {
void argmax(const RMatrix& m, std::size_t& row, std::size_t& col) {
  row = col = 0;
  for (std::size_t r = 0; r < m.rows(); ++r)
    for (std::size_t c = 0; c < m.cols(); ++c)
      if (m(r, c) > m(row, col)) {
        row = r;
        col = c;
      }
}
}  // namespace

int main() {
  const RadarParams p;
  const PulseCompressor pc(make_lfm_chirp(p), p.num_samples);

  // 1) Bilinen menzil/hız: hücre merkezinde olacak şekilde seçilen hedef beklenen hücrede çıkar.
  for (int bins : {-10, 5, 17}) {
    CMatrix frame(p.num_pulses, p.num_samples);
    Target t;
    t.range_m = 100.0 * p.range_bin_m();
    t.velocity_mps = bins * p.velocity_resolution_mps();
    add_target(frame, p, t);
    const RMatrix rd = range_doppler_power(pc.compress(frame));
    std::size_t row, col;
    argmax(rd, row, col);
    CHECK(col == 100);
    CHECK(static_cast<int>(row) == static_cast<int>(p.num_pulses / 2) + bins);
    CHECK_NEAR(velocity_of_bin(p, row), t.velocity_mps, 1e-6);
    CHECK_NEAR(range_of_bin(p, col), t.range_m, 1e-6);
  }

  // 2) Pencere yan lobları bastırır: Hann ile Doppler ekseninde uzak bin'ler
  //    dikdörtgen pencereye göre (hız bin'e oturmadığında) çok daha düşüktür.
  {
    CMatrix frame(p.num_pulses, p.num_samples);
    Target t;
    t.range_m = 100.0 * p.range_bin_m();
    t.velocity_mps = 5.5 * p.velocity_resolution_mps();  // bin'ler arasında -> sızıntı
    add_target(frame, p, t);
    const CMatrix c = pc.compress(frame);
    const RMatrix rect = range_doppler_power(c, WindowType::Rectangular);
    const RMatrix hann = range_doppler_power(c, WindowType::Hann);
    const std::size_t far_row = p.num_pulses / 2 + 5 + 15;  // tepeden 15 bin uzak
    const double rect_rel = rect(far_row, 100) / rect(p.num_pulses / 2 + 5, 100);
    const double hann_rel = hann(far_row, 100) / hann(p.num_pulses / 2 + 5, 100);
    CHECK(hann_rel < 0.1 * rect_rel);
  }

  // 3) Pencere şekilleri ve dB dönüşümü.
  {
    const std::vector<double> w = make_window(WindowType::Hann, 64);
    CHECK_NEAR(w.front(), 0.0, 1e-12);
    CHECK_NEAR(w[31] > 0.99 ? 1.0 : 0.0, 1.0, 0.0);
    RMatrix pw(1, 2);
    pw(0, 0) = 100.0;
    pw(0, 1) = 1.0;
    const RMatrix db = power_to_db(pw);
    CHECK_NEAR(db(0, 0), 20.0, 1e-9);
    CHECK_NEAR(db(0, 1), 0.0, 1e-9);
  }

  return testing::test_summary("test_range_doppler");
}
