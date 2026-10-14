// pulse_compression.h — Eşleşik filtre (matched filter) ile darbe sıkıştırma.
//
// Problem: Uzun bir chirp (10 us) çok enerji taşır ama zamanda yayılmıştır; iki yakın
// hedefin yankısı birbirine karışır. Çözüm: alınan sinyali gönderilen chirp'in
// eşleniği ile korele et. Sonuç, genişliği ~1/B olan dar bir tepedir:
//   * menzil çözünürlüğü  dR = c / (2B)  (chirp süresinden BAĞIMSIZ)
//   * SNR kazancı         L = chirp örnek sayısı (tüm enerji tek tepede toplanır)
//
// Korelasyon:   y[n] = sum_m x[n + m] * conj(s[m])
// Frekans bölgesinde (korelasyon teoremi):  Y[k] = X[k] * conj(S[k])
// yani FFT -> çarp -> IFFT, maliyet O(N log N) (doğrudan korelasyon O(N*L)).
//
// Tepe konumu: y[n0] büyük olur, n0 = yankının başladığı örnek. Menzil = n0 * c / (2 fs).
// Menzil yan lobları: dikdörtgen zarflı LFM chirp'in sıkıştırılmış çıktısının yan lobları
// sadece -13 dB'dir. Sıfır Doppler'deki güçlü clutter'ın yan lobları menzil boyunca yayılıp
// sahte tespit üretir. Çözüm (gerçek radarlarda da kullanılır): referansa Hamming ağırlığı
// uygulamak -> yan loblar ~-40 dB'ye iner. Bedel: ana lob ~%50 genişler (çözünürlük
// kaybı) ve ~1.3 dB SNR kaybı (uyumsuz filtre). Varsayılan Hamming; Rectangular = klasik
// eşleşik filtre (tepe = A*L tam).
//
// Not: dairesel (circular) korelasyon kullanıyoruz; pencere sonuna taşan yankılar
// başa sarar. Hedefleri pencerenin son chirp_samples() örneğinden uzak tutmak gerekir.
#pragma once

#include <vector>

#include "matrix.h"
#include "radar_params.h"
#include "range_doppler.h"  // WindowType
#include "signal_gen.h"

namespace radar {

class PulseCompressor {
 public:
  // reference: gönderilen chirp. num_samples: darbe başına örnek (2'nin kuvveti).
  PulseCompressor(const Chirp& reference, std::size_t num_samples,
                  WindowType weighting = WindowType::Hamming);

  // Tek darbeyi (num_samples uzunluğunda) yerinde sıkıştırır.
  void compress(std::vector<Complex>& pulse) const;

  // Tüm darbeleri (satırları) sıkıştırır; yeni matris döner.
  CMatrix compress(const CMatrix& frame) const;

 private:
  std::vector<Complex> conj_reference_spectrum_;  // conj(FFT(sıfır dolgulu chirp))
};

}  // namespace radar
