// range_doppler.h — Range-Doppler haritası: sıkıştırılmış veriden menzil x hız görüntüsü.
//
// Girdi: darbe sıkıştırma sonrası matris (satır = darbe/slow-time, sütun = menzil hücresi).
// Her menzil hücresi için darbeler boyunca (sütun yönünde) bir FFT alırız:
//   hareketli hedefin darbeden darbeye dönen fazı exp(j 2 pi fd m / PRF) bir "sinüs"tür,
//   FFT bu sinüsün frekansını (Doppler fd) bir tepe olarak gösterir.
//
// Doppler bini -> hız:   fd = (k - N/2) * PRF / N   (fftshift sonrası),  v = fd * lambda / 2
// Pozitif hız = yaklaşan hedef = pozitif Doppler.
//
// Pencereleme: Pencere kullanılmazsa (dikdörtgen) FFT yan lobları sadece -13 dB'dir;
// güçlü bir hedefin yan lobu zayıf komşu hedefi / CFAR eşiğini bozar. Hann penceresi
// yan lobları ~-31 dB'ye indirir, bedeli ana lobun biraz genişlemesi ve ~1.5 dB SNR kaybıdır.
//
// Çıktı satır = Doppler bini (0 = -PRF/2, N/2 = sıfır hız), sütun = menzil hücresi.
#pragma once

#include <string>
#include <vector>

#include "matrix.h"
#include "radar_params.h"

namespace radar {

enum class WindowType { Rectangular, Hann, Hamming };

std::vector<double> make_window(WindowType type, std::size_t n);

// Doğrusal GÜÇ haritası |X|^2 (CFAR doğrusal güç üzerinde çalışır).
RMatrix range_doppler_power(const CMatrix& compressed, WindowType window = WindowType::Hann);

// Güç -> dB (10 log10). Sıfır güç için küçük bir taban kullanılır.
RMatrix power_to_db(const RMatrix& power);

// Eksen yardımcıları: hücre indeksinden fiziksel büyüklüğe.
double range_of_bin(const RadarParams& p, std::size_t range_bin);
double velocity_of_bin(const RadarParams& p, std::size_t doppler_bin);

// CSV (satır = Doppler, sütun = menzil) olarak kaydeder. Başarılıysa true.
bool write_csv(const RMatrix& map, const std::string& path);

}  // namespace radar
