// matrix.h — Küçük, satır-öncelikli (row-major) 2B dizi sınıfı.
//
// Neden kendi matrisimiz? Harici kütüphane (Eigen vb.) eklemeden, verinin bellekte
// nasıl durduğunu açıkça görmek istiyoruz. Radar verisi "darbe x örnek" bir tablodur:
//   satır   = darbe (slow-time, darbeler arası zaman)
//   sütun   = örnek (fast-time, bir darbe içindeki zaman  ->  menzil)
// Satırlar bellekte ardışık olduğu için bir darbenin tüm örneklerine hızlı erişilir.
#pragma once

#include <cassert>
#include <complex>
#include <cstddef>
#include <vector>

namespace radar {

using Complex = std::complex<double>;

template <typename T>
class Matrix {
 public:
  Matrix() = default;
  Matrix(std::size_t rows, std::size_t cols, T init = T{})
      : rows_(rows), cols_(cols), data_(rows * cols, init) {}

  std::size_t rows() const { return rows_; }
  std::size_t cols() const { return cols_; }

  T& operator()(std::size_t r, std::size_t c) {
    assert(r < rows_ && c < cols_);
    return data_[r * cols_ + c];
  }
  const T& operator()(std::size_t r, std::size_t c) const {
    assert(r < rows_ && c < cols_);
    return data_[r * cols_ + c];
  }

  // Bir satırın ilk elemanına işaretçi (satır bellekte ardışıktır).
  T* row(std::size_t r) { return data_.data() + r * cols_; }
  const T* row(std::size_t r) const { return data_.data() + r * cols_; }

  std::vector<T>& data() { return data_; }
  const std::vector<T>& data() const { return data_; }

 private:
  std::size_t rows_ = 0;
  std::size_t cols_ = 0;
  std::vector<T> data_;
};

using CMatrix = Matrix<Complex>;  // karmaşık I/Q verisi
using RMatrix = Matrix<double>;   // gerçek değerli harita (güç, dB ...)

}  // namespace radar
