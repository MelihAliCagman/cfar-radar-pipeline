// radar_params.h — Radar sisteminin tasarım parametreleri ve türetilen büyüklükler.
//
// Tüm modüller aynı RadarParams nesnesini paylaşır; böylece "bu hücre kaç metre?"
// ya da "bu Doppler bini kaç m/s?" sorularının cevabı tek yerde durur.
//
// Temel ilişkiler (kodun geri kalanında hep bunlara dönüyoruz):
//
//   dalga boyu            lambda = c / fc
//   menzil çözünürlüğü    dR = c / (2 B)            (B = chirp bant genişliği)
//   örnek başına menzil   c / (2 fs)                (fs = örnekleme frekansı)
//   Doppler kayması       fd = 2 v / lambda         (v: yaklaşma hızı)
//   hız çözünürlüğü       dv = lambda * PRF / (2 N) (N = darbe sayısı)
//   belirsiz hız sınırı   |v| < lambda * PRF / 4    (fd, PRF'i aşınca katlanır)
//
// İşaret kuralı: hız POZİTİF = hedef radara YAKLAŞIYOR (pozitif Doppler).
#pragma once

#include <cmath>
#include <cstddef>

namespace radar {

constexpr double kPi = 3.14159265358979323846;
constexpr double kSpeedOfLight = 299792458.0;  // m/s

struct RadarParams {
  double carrier_hz = 10e9;      // X-band, dalga boyu ~3 cm
  double bandwidth_hz = 10e6;    // LFM chirp bant genişliği
  double sample_rate_hz = 20e6;  // karmaşık (I/Q) örnekleme; B'den büyük olmalı
  double pulse_width_s = 10e-6;  // chirp süresi
  double prf_hz = 10e3;          // darbe tekrar frekansı
  std::size_t num_pulses = 64;   // bir CPI'daki darbe sayısı (2'nin kuvveti: FFT)
  std::size_t num_samples = 512; // darbe başına alınan örnek (2'nin kuvveti: FFT)

  double wavelength() const { return kSpeedOfLight / carrier_hz; }

  // Sıkıştırma sonrası iki hedefi ayırabildiğimiz en küçük menzil farkı.
  double range_resolution_m() const { return kSpeedOfLight / (2.0 * bandwidth_hz); }

  // Bir örnek (menzil hücresi) kaç metreye karşılık gelir.
  double range_bin_m() const { return kSpeedOfLight / (2.0 * sample_rate_hz); }

  double velocity_resolution_mps() const {
    return wavelength() * prf_hz / (2.0 * static_cast<double>(num_pulses));
  }
  double max_unambiguous_velocity_mps() const { return wavelength() * prf_hz / 4.0; }

  // Chirp'in örnek sayısı (darbe genişliği x fs).
  std::size_t chirp_samples() const {
    return static_cast<std::size_t>(std::llround(pulse_width_s * sample_rate_hz));
  }

  // Chirp eğimi k = B / Tp  (Hz/s). Anlık frekans f(t) = k * t.
  double chirp_rate() const { return bandwidth_hz / pulse_width_s; }

  // Pulse compression SNR kazancı = chirp örnek sayısı (eşleşik filtre).
  double compression_gain() const { return static_cast<double>(chirp_samples()); }
};

}  // namespace radar
