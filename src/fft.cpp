#include "fft.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "radar_params.h"

namespace radar {

bool is_power_of_two(std::size_t n) { return n != 0 && (n & (n - 1)) == 0; }

namespace {

// Bit-reversal permütasyonu: indeksin bitlerini tersine çevirerek elemanları yer değiştirir.
// Örn. N=8:  1 (001) <-> 4 (100),  3 (011) <-> 6 (110).
// Özyinelemeli "çift/tek ayır" işleminin sonunda elemanlar tam bu sırada kalır.
void bit_reverse_permute(std::vector<Complex>& x) {
  const std::size_t n = x.size();
  for (std::size_t i = 1, j = 0; i < n; ++i) {
    std::size_t bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(x[i], x[j]);
  }
}

void fft_core(std::vector<Complex>& x, bool inverse) {
  const std::size_t n = x.size();
  if (!is_power_of_two(n)) throw std::invalid_argument("fft: length must be a power of two");
  if (n == 1) return;

  bit_reverse_permute(x);

  // len = o aşamada birleştirilen alt dizinin uzunluğu: 2, 4, 8, ... N
  for (std::size_t len = 2; len <= n; len <<= 1) {
    // Ters dönüşümde dönüş yönü ters (e^{+j...}).
    const double angle = (inverse ? 2.0 : -2.0) * kPi / static_cast<double>(len);
    const Complex w_len(std::cos(angle), std::sin(angle));

    for (std::size_t start = 0; start < n; start += len) {
      Complex w(1.0, 0.0);  // W^k, her adımda w_len ile çarpılarak güncellenir
      for (std::size_t k = 0; k < len / 2; ++k) {
        // Kelebek: iki yarıdan (E ve W^k*O) toplam ve fark üretir.
        const Complex even = x[start + k];
        const Complex odd = w * x[start + k + len / 2];
        x[start + k] = even + odd;
        x[start + k + len / 2] = even - odd;
        w *= w_len;
      }
    }
  }
}

}  // namespace

void fft(std::vector<Complex>& x) { fft_core(x, false); }

void ifft(std::vector<Complex>& x) {
  fft_core(x, true);
  const double scale = 1.0 / static_cast<double>(x.size());
  for (Complex& v : x) v *= scale;
}

std::vector<Complex> dft_naive(const std::vector<Complex>& x) {
  const std::size_t n = x.size();
  std::vector<Complex> out(n);
  for (std::size_t k = 0; k < n; ++k) {
    Complex sum(0.0, 0.0);
    for (std::size_t m = 0; m < n; ++m) {
      const double angle = -2.0 * kPi * static_cast<double>(k * m % n) / static_cast<double>(n);
      sum += x[m] * Complex(std::cos(angle), std::sin(angle));
    }
    out[k] = sum;
  }
  return out;
}

void fftshift(std::vector<Complex>& x) {
  std::rotate(x.begin(), x.begin() + static_cast<std::ptrdiff_t>((x.size() + 1) / 2), x.end());
}

}  // namespace radar
