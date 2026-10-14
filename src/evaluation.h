// evaluation.h — Monte Carlo değerlendirme: Pd, gerçekleşen Pfa ve işlem süresi.
//
// Pd  (detection probability)  = hedefin doğru hücrede (±1 hücre) tespit edildiği deneme oranı.
// Pfa (false alarm probability) = yalnızca gürültü verisinde, test edilen hücrelerin kaçı eşiği aştı.
//
// CA ve OS-CFAR AYNI simüle çerçeveler üzerinde çalıştırılır (eşleştirilmiş karşılaştırma):
// fark yalnızca dedektörden gelir, rastgelelikten değil.
//
// Not: tasarım Pfa'sı "bağımsız üstel gürültü hücreleri" varsayar. Gerçek hatta darbe
// sıkıştırma (fs = 2B -> komşu menzil hücreleri ilintili) ve Hann penceresi (komşu Doppler
// hücreleri ilintili) gürültüyü ilintilendirir; bu yüzden ölçülen Pfa tasarımdan biraz sapar.
// Bunu ölçmek bu modülün amaçlarından biridir.
#pragma once

#include <cstdint>
#include <string>

#include "cfar.h"
#include "radar_params.h"

namespace radar {

enum class Scenario {
  Homogeneous,         // yalnızca gürültü + tek hedef
  ClutterEdge,         // hedef, bir clutter bölgesinin kenarına yakın
  InterferingTarget,   // hedefin eğitim penceresinde 15 dB daha güçlü ikinci hedef
};

const char* scenario_name(Scenario s);

struct PdPair {
  double ca = 0.0;
  double os = 0.0;
};

// snr_db: bütünleştirme sonrası (sıkıştırma + darbe toplama) hedef SNR'ı.
PdPair measure_pd(const RadarParams& radar, const CfarParams& cfar, Scenario scenario,
                  double snr_db, int trials, std::uint64_t seed);

// Yalnızca gürültü; döndürülen değerler test edilen hücre başına yanlış alarm oranıdır.
PdPair measure_pfa(const RadarParams& radar, const CfarParams& cfar, int frames,
                   std::uint64_t seed);

struct Timing {
  double generate_ms = 0, compress_ms = 0, range_doppler_ms = 0, ca_cfar_ms = 0, os_cfar_ms = 0;
};

// Bir çerçeve için her aşamanın ortalama süresi (ms).
Timing measure_timing(const RadarParams& radar, const CfarParams& cfar, int frames,
                      std::uint64_t seed);

}  // namespace radar
