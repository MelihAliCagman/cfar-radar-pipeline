#include "evaluation.h"

#include <chrono>
#include <cmath>
#include <random>

#include "pulse_compression.h"
#include "range_doppler.h"
#include "signal_gen.h"

namespace radar {

const char* scenario_name(Scenario s) {
  switch (s) {
    case Scenario::Homogeneous: return "homogeneous";
    case Scenario::ClutterEdge: return "clutter_edge";
    case Scenario::InterferingTarget: return "interfering_target";
  }
  return "?";
}

namespace {

constexpr double kNoisePower = 1.0;  // gürültü gücü 1 seçilir; diğer her şey buna göre ölçeklenir

RMatrix process_frame(const PulseCompressor& pc, const CMatrix& frame) {
  return range_doppler_power(pc.compress(frame), WindowType::Hann);
}

// Hedefin tespit sayılması için: doğru hücrenin ±1 komşuluğunda bir tespit olmalı.
bool hit_near(const std::vector<Detection>& dets, long range_bin, long doppler_bin) {
  for (const Detection& d : dets) {
    if (std::labs(static_cast<long>(d.range_bin) - range_bin) <= 1 &&
        std::labs(static_cast<long>(d.doppler_bin) - doppler_bin) <= 1)
      return true;
  }
  return false;
}

}  // namespace

PdPair measure_pd(const RadarParams& radar, const CfarParams& cfar, Scenario scenario,
                  double snr_db, int trials, std::uint64_t seed) {
  std::mt19937_64 rng(seed);
  const PulseCompressor pc(make_lfm_chirp(radar), radar.num_samples);
  const std::size_t margin = cfar.guard_range + cfar.train_range;

  const double v_res = radar.velocity_resolution_mps();
  std::uniform_int_distribution<long> bin_dist(static_cast<long>(margin) + 20,
                                               static_cast<long>(radar.num_samples) -
                                                   static_cast<long>(radar.chirp_samples()) - 20);
  std::uniform_real_distribution<double> frac(0.0, 1.0);
  std::uniform_real_distribution<double> speed_bins(6.0, 24.0);  // sıfır Doppler'den uzak
  std::bernoulli_distribution coin(0.5);

  int hits_ca = 0, hits_os = 0;
  for (int t = 0; t < trials; ++t) {
    CMatrix frame(radar.num_pulses, radar.num_samples);

    Target target;
    const double range_cells = static_cast<double>(bin_dist(rng)) + frac(rng);  // kesirli gecikme
    target.range_m = range_cells * radar.range_bin_m();
    const double v_bins = speed_bins(rng) * (coin(rng) ? 1.0 : -1.0);  // kesirli Doppler bini
    target.velocity_mps = v_bins * v_res;
    target.amplitude = amplitude_for_integrated_snr(radar, snr_db, kNoisePower);
    target.phase_rad = 2.0 * kPi * frac(rng);
    add_target(frame, radar, target);

    const long truth_range = static_cast<long>(std::ceil(range_cells));
    const long truth_doppler = static_cast<long>(radar.num_pulses / 2) + std::lround(v_bins);

    if (scenario == Scenario::ClutterEdge) {
      // Clutter bölgesi hedefin biraz ötesinde biter: hedefin eğitim penceresinin bir kısmı
      // clutter'da, bir kısmı temiz gürültüde kalır (menzil kenarı).
      ClutterPatch patch;
      patch.start_range_m = (range_cells - 60.0) * radar.range_bin_m();
      patch.end_range_m = (range_cells + 3.0) * radar.range_bin_m();
      patch.scatterer_snr_db = 0.0;
      patch.velocity_sigma_mps = 1.0;  // geniş Doppler yayılımı: clutter hedefin Doppler'ine de sızar
      add_clutter_patch(frame, radar, patch, kNoisePower, rng);
    } else if (scenario == Scenario::InterferingTarget) {
      Target strong = target;
      strong.range_m = (range_cells + 5.0) * radar.range_bin_m();  // eğitim penceresinin içi
      strong.amplitude = amplitude_for_integrated_snr(radar, snr_db + 15.0, kNoisePower);
      strong.phase_rad = 2.0 * kPi * frac(rng);
      add_target(frame, radar, strong);
    }
    add_noise(frame, kNoisePower, rng);

    const RMatrix rd = process_frame(pc, frame);
    if (hit_near(cfar_detect(rd, radar, cfar, CfarType::CellAveraging), truth_range, truth_doppler))
      ++hits_ca;
    if (hit_near(cfar_detect(rd, radar, cfar, CfarType::OrderedStatistic), truth_range,
                 truth_doppler))
      ++hits_os;
  }
  return {static_cast<double>(hits_ca) / trials, static_cast<double>(hits_os) / trials};
}

PdPair measure_pfa(const RadarParams& radar, const CfarParams& cfar_in, int frames,
                   std::uint64_t seed) {
  CfarParams cfar = cfar_in;
  cfar.local_maxima_only = false;  // her eşik aşımı bir yanlış alarmdır
  std::mt19937_64 rng(seed);
  const PulseCompressor pc(make_lfm_chirp(radar), radar.num_samples);
  const std::size_t margin = cfar.guard_range + cfar.train_range;
  const double tested_per_frame =
      static_cast<double>(radar.num_pulses * (radar.num_samples - 2 * margin));

  double ca = 0.0, os = 0.0;
  for (int f = 0; f < frames; ++f) {
    CMatrix frame(radar.num_pulses, radar.num_samples);
    add_noise(frame, kNoisePower, rng);
    const RMatrix rd = process_frame(pc, frame);
    ca += static_cast<double>(cfar_detect(rd, radar, cfar, CfarType::CellAveraging).size());
    os += static_cast<double>(cfar_detect(rd, radar, cfar, CfarType::OrderedStatistic).size());
  }
  return {ca / (frames * tested_per_frame), os / (frames * tested_per_frame)};
}

Timing measure_timing(const RadarParams& radar, const CfarParams& cfar, int frames,
                      std::uint64_t seed) {
  using Clock = std::chrono::steady_clock;
  auto ms = [](Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
  };

  std::mt19937_64 rng(seed);
  const PulseCompressor pc(make_lfm_chirp(radar), radar.num_samples);
  Timing sum;
  for (int f = 0; f < frames; ++f) {
    const auto t0 = Clock::now();
    CMatrix frame(radar.num_pulses, radar.num_samples);
    for (int i = 0; i < 3; ++i) {
      Target tg;
      tg.range_m = (100.0 + 90.0 * i) * radar.range_bin_m();
      tg.velocity_mps = (i - 1) * 10.0;
      tg.amplitude = amplitude_for_integrated_snr(radar, 25.0, kNoisePower);
      add_target(frame, radar, tg);
    }
    add_noise(frame, kNoisePower, rng);
    const auto t1 = Clock::now();
    const CMatrix compressed = pc.compress(frame);
    const auto t2 = Clock::now();
    const RMatrix rd = range_doppler_power(compressed, WindowType::Hann);
    const auto t3 = Clock::now();
    const auto d1 = cfar_detect(rd, radar, cfar, CfarType::CellAveraging);
    const auto t4 = Clock::now();
    const auto d2 = cfar_detect(rd, radar, cfar, CfarType::OrderedStatistic);
    const auto t5 = Clock::now();
    (void)d1;
    (void)d2;
    sum.generate_ms += ms(t0, t1);
    sum.compress_ms += ms(t1, t2);
    sum.range_doppler_ms += ms(t2, t3);
    sum.ca_cfar_ms += ms(t3, t4);
    sum.os_cfar_ms += ms(t4, t5);
  }
  const double n = frames;
  return {sum.generate_ms / n, sum.compress_ms / n, sum.range_doppler_ms / n,
          sum.ca_cfar_ms / n, sum.os_cfar_ms / n};
}

}  // namespace radar
