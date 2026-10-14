// fft.h — Radix-2 (Cooley-Tukey) hızlı Fourier dönüşümü.
//
// Neden FFT? Radar hattında iki yerde kullanılır:
//   1) Darbe sıkıştırma: korelasyon = frekans bölgesinde çarpma (O(N log N)).
//   2) Range-Doppler: darbeler boyunca FFT hızı (Doppler frekansını) verir.
//
// Fikir: N noktalı DFT,  X[k] = sum_n x[n] exp(-j 2 pi k n / N),  doğrudan hesaplanırsa
// O(N^2). Çift ve tek indeksli örnekleri ayrı N/2 noktalı DFT'ler olarak ele alıp
//   X[k]       = E[k] + W^k O[k]
//   X[k + N/2] = E[k] - W^k O[k],      W = exp(-j 2 pi / N)
// ile birleştirirsek, bunu log2(N) kez özyineleyerek O(N log N) elde ederiz.
// Bu dosyadaki sürüm özyinelemesiz (iterative): önce bit-reversal ile diziyi yeniden
// sıralar, sonra "kelebek" (butterfly) aşamalarını küçükten büyüğe uygular.
#pragma once

#include <cstddef>
#include <vector>

#include "matrix.h"

namespace radar {

bool is_power_of_two(std::size_t n);

// Yerinde (in-place) FFT. x.size() 2'nin kuvveti olmalı, değilse std::invalid_argument.
//  * Düz dönüşüm ölçeklenmez:  X[k] = sum x[n] e^{-j2pi kn/N}
//  * Ters dönüşüm 1/N ile ölçeklenir, böylece ifft(fft(x)) == x.
void fft(std::vector<Complex>& x);
void ifft(std::vector<Complex>& x);

// Referans amaçlı O(N^2) DFT: FFT'nin doğruluğunu test etmek için.
std::vector<Complex> dft_naive(const std::vector<Complex>& x);

// Sıfır frekansı dizinin ortasına taşır (Doppler eksenini -PRF/2..+PRF/2 yapmak için).
// Çift uzunluk için fftshift kendi tersidir.
void fftshift(std::vector<Complex>& x);

}  // namespace radar
