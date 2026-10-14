#include "pulse_compression.h"

#include <algorithm>
#include <stdexcept>

#include "fft.h"

namespace radar {

PulseCompressor::PulseCompressor(const Chirp& reference, std::size_t num_samples) {
  if (reference.size() > num_samples)
    throw std::invalid_argument("PulseCompressor: chirp longer than pulse window");

  // Referansı darbe uzunluğuna sıfırla doldur, bir kez FFT al ve eşleniğini sakla.
  // Her darbede yeniden hesaplamak gereksiz: referans hiç değişmiyor.
  conj_reference_spectrum_.assign(num_samples, Complex(0.0, 0.0));
  std::copy(reference.begin(), reference.end(), conj_reference_spectrum_.begin());
  fft(conj_reference_spectrum_);
  for (Complex& v : conj_reference_spectrum_) v = std::conj(v);
}

void PulseCompressor::compress(std::vector<Complex>& pulse) const {
  if (pulse.size() != conj_reference_spectrum_.size())
    throw std::invalid_argument("PulseCompressor: pulse length mismatch");

  fft(pulse);
  for (std::size_t k = 0; k < pulse.size(); ++k) pulse[k] *= conj_reference_spectrum_[k];
  ifft(pulse);
}

CMatrix PulseCompressor::compress(const CMatrix& frame) const {
  CMatrix out(frame.rows(), frame.cols());
  std::vector<Complex> pulse(frame.cols());
  for (std::size_t m = 0; m < frame.rows(); ++m) {
    std::copy(frame.row(m), frame.row(m) + frame.cols(), pulse.begin());
    compress(pulse);
    std::copy(pulse.begin(), pulse.end(), out.row(m));
  }
  return out;
}

}  // namespace radar
