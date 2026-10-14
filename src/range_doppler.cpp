#include "range_doppler.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "fft.h"

namespace radar {

std::vector<double> make_window(WindowType type, std::size_t n) {
  std::vector<double> w(n, 1.0);
  if (type == WindowType::Rectangular || n < 2) return w;
  // Hann:    0.5  - 0.5  cos(2 pi i/(N-1))
  // Hamming: 0.54 - 0.46 cos(2 pi i/(N-1))
  const double a0 = (type == WindowType::Hann) ? 0.5 : 0.54;
  for (std::size_t i = 0; i < n; ++i)
    w[i] = a0 - (1.0 - a0) * std::cos(2.0 * kPi * static_cast<double>(i) / static_cast<double>(n - 1));
  return w;
}

RMatrix range_doppler_power(const CMatrix& compressed, WindowType window) {
  const std::size_t num_pulses = compressed.rows();
  const std::size_t num_ranges = compressed.cols();
  const std::vector<double> w = make_window(window, num_pulses);

  RMatrix power(num_pulses, num_ranges);
  std::vector<Complex> slow_time(num_pulses);
  for (std::size_t r = 0; r < num_ranges; ++r) {
    // Bu menzil hücresinin darbeler boyunca örnekleri (sütun okuma), pencereli.
    for (std::size_t m = 0; m < num_pulses; ++m) slow_time[m] = compressed(m, r) * w[m];
    fft(slow_time);
    fftshift(slow_time);  // sıfır Doppler ortaya
    for (std::size_t k = 0; k < num_pulses; ++k) power(k, r) = std::norm(slow_time[k]);
  }
  return power;
}

RMatrix power_to_db(const RMatrix& power) {
  RMatrix db(power.rows(), power.cols());
  for (std::size_t i = 0; i < power.data().size(); ++i)
    db.data()[i] = 10.0 * std::log10(std::max(power.data()[i], 1e-30));
  return db;
}

double range_of_bin(const RadarParams& p, std::size_t range_bin) {
  return static_cast<double>(range_bin) * p.range_bin_m();
}

double velocity_of_bin(const RadarParams& p, std::size_t doppler_bin) {
  const double n = static_cast<double>(p.num_pulses);
  const double fd = (static_cast<double>(doppler_bin) - n / 2.0) * p.prf_hz / n;
  return fd * p.wavelength() / 2.0;
}

bool write_csv(const RMatrix& map, const std::string& path) {
  std::FILE* f = std::fopen(path.c_str(), "w");
  if (!f) return false;
  for (std::size_t r = 0; r < map.rows(); ++r) {
    for (std::size_t c = 0; c < map.cols(); ++c)
      std::fprintf(f, c ? ",%.3f" : "%.3f", map(r, c));
    std::fputc('\n', f);
  }
  std::fclose(f);
  return true;
}

}  // namespace radar
