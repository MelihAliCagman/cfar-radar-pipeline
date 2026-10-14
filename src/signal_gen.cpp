#include "signal_gen.h"

#include <algorithm>
#include <cmath>

namespace radar {

Complex chirp_at(const RadarParams& p, double t) {
  if (t < 0.0 || t >= p.pulse_width_s) return {0.0, 0.0};
  // Faz = pi*k*(t - Tp/2)^2  ->  türevi (anlık frekans) k*(t - Tp/2): -B/2'den +B/2'ye.
  const double tc = t - 0.5 * p.pulse_width_s;
  const double phase = kPi * p.chirp_rate() * tc * tc;
  return {std::cos(phase), std::sin(phase)};
}

Chirp make_lfm_chirp(const RadarParams& p) {
  Chirp chirp(p.chirp_samples());
  for (std::size_t n = 0; n < chirp.size(); ++n) {
    chirp[n] = chirp_at(p, static_cast<double>(n) / p.sample_rate_hz);
  }
  return chirp;
}

void add_target(CMatrix& frame, const RadarParams& p, const Target& target) {
  const double fs = p.sample_rate_hz;
  const double delay_s = 2.0 * target.range_m / kSpeedOfLight;  // gidiş-dönüş gecikmesi
  const std::size_t first_sample =
      static_cast<std::size_t>(std::max(0.0, std::ceil(delay_s * fs)));

  for (std::size_t m = 0; m < frame.rows(); ++m) {
    // Hedef yaklaşıyorsa menzil her darbede azalır. Bunu yalnızca FAZ için kullanıyoruz:
    // bir CPI boyunca menzil değişimi (cm mertebesi) örnek aralığından (7.5 m) çok küçük.
    const double range_m =
        target.range_m - target.velocity_mps * static_cast<double>(m) / p.prf_hz;
    const double carrier_phase =
        -2.0 * kPi * p.carrier_hz * (2.0 * range_m / kSpeedOfLight) + target.phase_rad;
    const Complex gain =
        target.amplitude * Complex(std::cos(carrier_phase), std::sin(carrier_phase));

    for (std::size_t n = first_sample; n < frame.cols(); ++n) {
      const double t = static_cast<double>(n) / fs - delay_s;  // chirp'e göre yerel zaman
      if (t >= p.pulse_width_s) break;                         // yankı bitti
      frame(m, n) += gain * chirp_at(p, t);
    }
  }
}

void add_noise(CMatrix& frame, double noise_power, std::mt19937_64& rng) {
  // E[|n|^2] = sigma_I^2 + sigma_Q^2 = noise_power  ->  her bileşenin sigma'sı sqrt(P/2).
  std::normal_distribution<double> gauss(0.0, std::sqrt(noise_power / 2.0));
  for (Complex& x : frame.data()) x += Complex(gauss(rng), gauss(rng));
}

void add_clutter_patch(CMatrix& frame, const RadarParams& p, const ClutterPatch& patch,
                       double noise_power, std::mt19937_64& rng) {
  const double scatterer_power = noise_power * std::pow(10.0, patch.scatterer_snr_db / 10.0);
  std::normal_distribution<double> amp(0.0, std::sqrt(scatterer_power / 2.0));
  std::normal_distribution<double> vel(0.0, patch.velocity_sigma_mps);
  std::uniform_real_distribution<double> uniform_phase(0.0, 2.0 * kPi);

  for (double r = patch.start_range_m; r <= patch.end_range_m; r += p.range_bin_m()) {
    Target scatterer;
    scatterer.range_m = r;
    scatterer.velocity_mps = vel(rng);
    scatterer.amplitude = std::abs(Complex(amp(rng), amp(rng)));  // Rayleigh genlik
    scatterer.phase_rad = uniform_phase(rng);
    add_target(frame, p, scatterer);
  }
}

double amplitude_for_integrated_snr(const RadarParams& p, double snr_db, double noise_power) {
  const double total_gain = p.compression_gain() * static_cast<double>(p.num_pulses);
  const double snr_linear = std::pow(10.0, snr_db / 10.0);
  return std::sqrt(snr_linear * noise_power / total_gain);
}

}  // namespace radar
