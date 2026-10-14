// cfar.h — 2B CFAR (Constant False Alarm Rate) hedef tespiti: CA-CFAR ve OS-CFAR.
//
// Sabit bir eşik kullanamayız: gürültü/clutter seviyesi haritanın her yerinde farklı.
// CFAR, test edilen hücrenin (CUT) çevresindeki "eğitim" hücrelerinden yerel gürültü
// seviyesini kestirir ve eşiği buna göre kurar, böylece yanlış alarm oranı (Pfa) sabit kalır.
//
//        eğitim (train)            CUT'un hemen yanındaki "guard" hücreleri hedefin kendi
//     +-----------------+          enerjisinin (yan lob, sızıntı) gürültü kestirimine
//     |  T T T T T T T  |          karışmasını engeller, bu yüzden kestirimde kullanılmaz.
//     |  T G G G G G T  |
//     |  T G G C G G T  |          Hücre sayısı:  N = (2(gr+tr)+1)(2(gd+td)+1)
//     |  T G G G G G T  |                              - (2gr+1)(2gd+1)
//     |  T T T T T T T  |
//     +-----------------+
//
// Gürültü gücü (karmaşık Gauss -> |x|^2 üstel dağılım) için:
//
//  CA-CFAR : eşik = alpha * ortalama(eğitim),   alpha = N (Pfa^(-1/N) - 1)
//            Homojen gürültüde en iyi sonuç; ama clutter kenarında ve yakın hedeflerde
//            ortalama şişer, eşik yükselir ve hedef kaçırılır (masking).
//  OS-CFAR : eşik = T * (k. sıralı istatistik),   Pfa = prod_{i=0}^{k-1} (N-i)/(N-i+T)
//            Ortalama yerine sıralanmış eğitim hücrelerinin k'incisi (örn. %75) alınır;
//            birkaç güçlü komşu hedef/clutter uç değeri eşiği bozmaz. Bedel: sıralama maliyeti
//            ve homojen gürültüde hafif SNR kaybı.
//
// Doppler ekseni periyodiktir (belirsiz hız) -> kenarlarda sarılır. Menzil ekseninde
// pencere sığmayan kenar hücreleri test edilmez.
#pragma once

#include <vector>

#include "matrix.h"
#include "radar_params.h"

namespace radar {

enum class CfarType { CellAveraging, OrderedStatistic };

struct CfarParams {
  std::size_t guard_range = 2, guard_doppler = 1;
  std::size_t train_range = 6, train_doppler = 3;
  double pfa = 1e-6;           // tasarım yanlış alarm olasılığı (hücre başına)
  double os_rank_fraction = 0.75;  // OS-CFAR: k = fraction * N
  bool local_maxima_only = true;   // komşu aşımlarını tek hedefe indir (3x3 tepe noktası)
};

struct Detection {
  std::size_t range_bin = 0, doppler_bin = 0;
  double range_m = 0.0, velocity_mps = 0.0;
  double power = 0.0;   // hücre gücü (doğrusal)
  double snr_db = 0.0;  // hücre gücü / yerel gürültü kestirimi
};

// Eğitim hücresi sayısı N.
std::size_t cfar_num_training_cells(const CfarParams& c);

// Eşik çarpanları (kapalı form / sayısal çözüm).
double ca_cfar_alpha(std::size_t n_train, double pfa);
double os_cfar_alpha(std::size_t n_train, std::size_t rank_k, double pfa);

// power: doğrusal güç haritası (satır = Doppler, sütun = menzil).
std::vector<Detection> cfar_detect(const RMatrix& power, const RadarParams& radar,
                                   const CfarParams& cfar, CfarType type);

}  // namespace radar
