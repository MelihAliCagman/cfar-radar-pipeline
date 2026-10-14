#include "cfar.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "range_doppler.h"

namespace radar {

std::size_t cfar_num_training_cells(const CfarParams& c) {
  const std::size_t outer = (2 * (c.guard_range + c.train_range) + 1) *
                            (2 * (c.guard_doppler + c.train_doppler) + 1);
  const std::size_t inner = (2 * c.guard_range + 1) * (2 * c.guard_doppler + 1);
  return outer - inner;
}

double ca_cfar_alpha(std::size_t n, double pfa) {
  // Üstel gürültüde Pfa = (1 + alpha/N)^(-N)  ->  alpha = N (Pfa^(-1/N) - 1)
  const double dn = static_cast<double>(n);
  return dn * (std::pow(pfa, -1.0 / dn) - 1.0);
}

double os_cfar_alpha(std::size_t n, std::size_t k, double pfa) {
  // Pfa(T) = prod_{i=0}^{k-1} (N-i)/(N-i+T) T'de monoton azalan -> ikiye bölme ile çöz.
  auto pfa_of = [&](double t) {
    double p = 1.0;
    for (std::size_t i = 0; i < k; ++i) {
      const double ni = static_cast<double>(n - i);
      p *= ni / (ni + t);
    }
    return p;
  };
  double lo = 0.0, hi = 1.0;
  while (pfa_of(hi) > pfa) hi *= 2.0;
  for (int it = 0; it < 100; ++it) {
    const double mid = 0.5 * (lo + hi);
    (pfa_of(mid) > pfa ? lo : hi) = mid;
  }
  return 0.5 * (lo + hi);
}

std::vector<Detection> cfar_detect(const RMatrix& power, const RadarParams& radar,
                                   const CfarParams& c, CfarType type) {
  const std::size_t rows = power.rows(), cols = power.cols();
  const std::size_t gr = c.guard_range, gd = c.guard_doppler;
  const std::size_t wr = gr + c.train_range;    // dış pencere yarı genişliği (menzil)
  const std::size_t wd = gd + c.train_doppler;  // dış pencere yarı genişliği (Doppler)
  if (cols < 2 * wr + 1 || rows < 2 * wd + 1)
    throw std::invalid_argument("cfar_detect: map smaller than CFAR window");

  const std::size_t n_train = cfar_num_training_cells(c);
  const std::size_t rank_k = std::clamp<std::size_t>(
      static_cast<std::size_t>(std::llround(c.os_rank_fraction * static_cast<double>(n_train))), 1,
      n_train);
  const double alpha = (type == CfarType::CellAveraging) ? ca_cfar_alpha(n_train, c.pfa)
                                                         : os_cfar_alpha(n_train, rank_k, c.pfa);

  // Doppler eksenini wd satır sararak genişlet: kenar hücreleri de tam pencereye sahip olur.
  const std::size_t ext_rows = rows + 2 * wd;
  RMatrix ext(ext_rows, cols);
  for (std::size_t r = 0; r < ext_rows; ++r) {
    const std::size_t src = (r + rows - wd % rows) % rows;
    std::copy(power.row(src), power.row(src) + cols, ext.row(r));
  }

  // CA için integral görüntü (summed-area table): herhangi bir dikdörtgenin toplamı O(1).
  // integral(r, c) = ext[0..r-1][0..c-1] toplamı.
  RMatrix integral;
  if (type == CfarType::CellAveraging) {
    integral = RMatrix(ext_rows + 1, cols + 1, 0.0);
    for (std::size_t r = 0; r < ext_rows; ++r)
      for (std::size_t q = 0; q < cols; ++q)
        integral(r + 1, q + 1) =
            ext(r, q) + integral(r, q + 1) + integral(r + 1, q) - integral(r, q);
  }
  auto rect_sum = [&](std::size_t r0, std::size_t r1, std::size_t c0, std::size_t c1) {
    return integral(r1 + 1, c1 + 1) - integral(r0, c1 + 1) - integral(r1 + 1, c0) +
           integral(r0, c0);  // [r0..r1] x [c0..c1] dahil
  };

  std::vector<double> cells;  // OS-CFAR için eğitim hücreleri
  cells.reserve(n_train);

  RMatrix hit(rows, cols, 0.0);
  RMatrix noise_est(rows, cols, 0.0);

  for (std::size_t d = 0; d < rows; ++d) {
    const std::size_t er = d + wd;  // genişletilmiş haritadaki satır
    for (std::size_t q = wr; q + wr < cols; ++q) {
      double noise;
      if (type == CfarType::CellAveraging) {
        const double outer = rect_sum(er - wd, er + wd, q - wr, q + wr);
        const double inner = rect_sum(er - gd, er + gd, q - gr, q + gr);
        noise = (outer - inner) / static_cast<double>(n_train);
        noise_est(d, q) = noise;
        if (power(d, q) > alpha * noise) hit(d, q) = 1.0;
      } else {
        cells.clear();
        for (std::size_t rr = er - wd; rr <= er + wd; ++rr) {
          const bool in_guard_rows = rr + gd >= er && rr <= er + gd;
          for (std::size_t cc = q - wr; cc <= q + wr; ++cc) {
            if (in_guard_rows && cc + gr >= q && cc <= q + gr) continue;  // guard + CUT
            cells.push_back(ext(rr, cc));
          }
        }
        std::nth_element(cells.begin(), cells.begin() + static_cast<std::ptrdiff_t>(rank_k - 1),
                         cells.end());
        noise = cells[rank_k - 1];
        noise_est(d, q) = noise;  // sıralı istatistik; eşik = alpha * noise
        if (power(d, q) > alpha * noise) hit(d, q) = 1.0;
      }
    }
  }

  // Tespitleri topla. SNR = hücre gücü / yerel ortalama gürültü kestirimi.
  std::vector<Detection> out;
  for (std::size_t d = 0; d < rows; ++d) {
    for (std::size_t q = 0; q < cols; ++q) {
      if (hit(d, q) == 0.0) continue;
      if (c.local_maxima_only) {
        bool is_peak = true;
        for (int dd = -1; dd <= 1 && is_peak; ++dd)
          for (int dq = -1; dq <= 1; ++dq) {
            if (dd == 0 && dq == 0) continue;
            const std::size_t nd = static_cast<std::size_t>(
                (static_cast<long>(d) + dd + static_cast<long>(rows)) % static_cast<long>(rows));  // Doppler sarılır
            const long nq = static_cast<long>(q) + dq;
            if (nq < 0 || nq >= static_cast<long>(cols)) continue;
            if (power(nd, static_cast<std::size_t>(nq)) > power(d, q)) {
              is_peak = false;
              break;
            }
          }
        if (!is_peak) continue;
      }
      Detection det;
      det.range_bin = q;
      det.doppler_bin = d;
      det.range_m = range_of_bin(radar, q);
      det.velocity_mps = velocity_of_bin(radar, d);
      det.power = power(d, q);
      // OS'de noise bir sıralı istatistik; aynı eşiği veren eşdeğer ortalama gürültü:
      //   alpha_ca * ortalama = alpha * noise
      const double equiv_mean = alpha * noise_est(d, q) / ca_cfar_alpha(n_train, c.pfa);
      det.snr_db = 10.0 * std::log10(std::max(det.power / std::max(equiv_mean, 1e-30), 1e-30));
      out.push_back(det);
    }
  }
  return out;
}

}  // namespace radar
