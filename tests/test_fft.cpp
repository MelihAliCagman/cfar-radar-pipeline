#include <cmath>
#include <random>
#include <stdexcept>

#include "fft.h"
#include "radar_params.h"
#include "test_util.h"

using namespace radar;

int main() {
  // 1) Bilinen sinüs: x[n] = exp(j 2 pi 5 n / 64) -> tek tepe, bin 5'te, yüksekliği N.
  {
    const std::size_t n = 64;
    std::vector<Complex> x(n);
    for (std::size_t i = 0; i < n; ++i) {
      const double a = 2.0 * kPi * 5.0 * static_cast<double>(i) / n;
      x[i] = Complex(std::cos(a), std::sin(a));
    }
    fft(x);
    std::size_t peak = 0;
    for (std::size_t k = 1; k < n; ++k)
      if (std::abs(x[k]) > std::abs(x[peak])) peak = k;
    CHECK(peak == 5);
    CHECK_NEAR(std::abs(x[5]), static_cast<double>(n), 1e-9);
    CHECK_NEAR(std::abs(x[6]), 0.0, 1e-9);
  }

  // 2) Rastgele veride FFT == naif DFT (farklı uzunluklar).
  {
    std::mt19937_64 rng(7);
    std::normal_distribution<double> g(0.0, 1.0);
    for (std::size_t n : {2u, 8u, 64u, 256u}) {
      std::vector<Complex> x(n);
      for (Complex& v : x) v = Complex(g(rng), g(rng));
      std::vector<Complex> fast = x;
      fft(fast);
      const std::vector<Complex> slow = dft_naive(x);
      double max_err = 0.0;
      for (std::size_t k = 0; k < n; ++k) max_err = std::max(max_err, std::abs(fast[k] - slow[k]));
      CHECK_NEAR(max_err, 0.0, 1e-9);
    }
  }

  // 3) ifft(fft(x)) == x.
  {
    std::mt19937_64 rng(3);
    std::normal_distribution<double> g(0.0, 1.0);
    std::vector<Complex> x(128);
    for (Complex& v : x) v = Complex(g(rng), g(rng));
    std::vector<Complex> y = x;
    fft(y);
    ifft(y);
    double max_err = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) max_err = std::max(max_err, std::abs(x[i] - y[i]));
    CHECK_NEAR(max_err, 0.0, 1e-12);
  }

  // 4) Parseval: sum |x|^2 == (1/N) sum |X|^2 (enerji korunur).
  {
    std::mt19937_64 rng(5);
    std::normal_distribution<double> g(0.0, 1.0);
    std::vector<Complex> x(64);
    for (Complex& v : x) v = Complex(g(rng), g(rng));
    double time_energy = 0.0;
    for (const Complex& v : x) time_energy += std::norm(v);
    fft(x);
    double freq_energy = 0.0;
    for (const Complex& v : x) freq_energy += std::norm(v);
    CHECK_NEAR(time_energy, freq_energy / 64.0, 1e-9);
  }

  // 5) fftshift: sıfır frekansı (indeks 0) ortaya (N/2) taşır.
  {
    std::vector<Complex> x(8);
    for (std::size_t i = 0; i < 8; ++i) x[i] = Complex(static_cast<double>(i), 0.0);
    fftshift(x);
    CHECK_NEAR(x[4].real(), 0.0, 0.0);  // eski 0. eleman
    CHECK_NEAR(x[0].real(), 4.0, 0.0);  // eski 4. eleman (Nyquist) en başa
  }

  // 6) 2'nin kuvveti olmayan uzunluk reddedilir.
  {
    bool threw = false;
    std::vector<Complex> x(6);
    try {
      fft(x);
    } catch (const std::invalid_argument&) {
      threw = true;
    }
    CHECK(threw);
  }

  return testing::test_summary("test_fft");
}
