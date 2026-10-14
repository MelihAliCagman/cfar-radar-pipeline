// signal_gen.h — Simüle radar verisi üretimi: LFM chirp, hedefler, gürültü, clutter.
//
// Bu modül "radarın anteninden gelen ham I/Q verisini" taklit eder. Çıktı bir
// CMatrix'tir: satır = darbe (slow-time), sütun = örnek (fast-time).
//
// Fiziksel model (basitleştirilmiş, "stop-and-hop"):
//  * Her darbede radar bir LFM chirp yollar:  s(t) = exp(j*pi*k*(t - Tp/2)^2), 0<=t<Tp.
//    Anlık frekans -B/2 ... +B/2 arasında doğrusal süpürülür.
//  * Menzildeki hedef, chirp'in 2R/c kadar GECİKMİŞ ve genliği A olan kopyasını
//    geri yansıtır. Gecikme kesirli olabilir; chirp'i analitik hesapladığımız için
//    kesirli gecikme sorunu yoktur.
//  * Taşıyıcı fazı exp(-j*2*pi*fc*2R/c) darbeden darbeye değişir çünkü hedef
//    hareket eder. Faz değişimi = Doppler: darbe m'de R_m = R0 - v*m/PRF olduğundan
//    faz her darbede 2*pi*fd/PRF kadar döner (fd = 2v/lambda). İşte hızı
//    "slow-time" boyunca FFT ile ölçebilmemizin nedeni budur.
//  * Hedefin darbeler arası menzil göçü (range migration) ihmal edilir.
#pragma once

#include <cstdint>
#include <random>
#include <vector>

#include "matrix.h"
#include "radar_params.h"

namespace radar {

using Chirp = std::vector<Complex>;

struct Target {
  double range_m = 0.0;       // başlangıç menzili (CPI'ın ilk darbesinde)
  double velocity_mps = 0.0;  // + = yaklaşıyor
  double amplitude = 1.0;     // alıcıdaki genlik (gürültü gücü 1 ise giriş SNR'ı A^2)
  double phase_rad = 0.0;     // ek sabit faz
};

// Clutter: yer/deniz yansıması gibi menzil boyunca yayılmış, hızı ~0 olan saçıcılar.
// Menzil hücresi başına bir saçıcı, hızları N(0, velocity_sigma^2) dağılır; bu yüzden
// range-Doppler haritasında sıfır Doppler çevresinde bir "şerit" oluşur.
struct ClutterPatch {
  double start_range_m = 0.0;
  double end_range_m = 0.0;
  double scatterer_snr_db = 0.0;    // saçıcı başına (sıkıştırma ÖNCESİ) SNR
  double velocity_sigma_mps = 0.3;  // iç hareket (rüzgârda sallanan yaprak vb.)
};

// Chirp'in t anındaki karmaşık değeri (0 <= t < Tp dışında 0).
Complex chirp_at(const RadarParams& p, double t);

// Eşleşik filtrenin referansı olan, fs ile örneklenmiş chirp (uzunluk = chirp_samples()).
Chirp make_lfm_chirp(const RadarParams& p);

// Tüm darbelere tek bir hareketli hedefin yankısını EKLER (üst üste binme: toplanır).
void add_target(CMatrix& frame, const RadarParams& p, const Target& target);

// Karmaşık Gauss gürültü ekler. noise_power = E[|n|^2] = I ve Q gücünün toplamı.
void add_noise(CMatrix& frame, double noise_power, std::mt19937_64& rng);

// Menzil hücresi başına bir saçıcı yerleştirerek clutter bölgesi ekler.
void add_clutter_patch(CMatrix& frame, const RadarParams& p, const ClutterPatch& patch,
                       double noise_power, std::mt19937_64& rng);

// Verilen "bütünleştirme sonrası SNR" için gereken hedef genliği.
// Bütünleştirme sonrası SNR = giriş SNR'ı x darbe sıkıştırma kazancı (L) x
// darbeler-arası tutarlı toplama kazancı (N). Gürültü gücü noise_power'dır.
double amplitude_for_integrated_snr(const RadarParams& p, double snr_db, double noise_power);

}  // namespace radar
