// main.cpp — radar_demo: simüle radar hattını uçtan uca çalıştırır.
//
//   1) Sahne: 3 hedef + clutter + gürültü   (signal_gen)
//   2) Darbe sıkıştırma                     (pulse_compression)
//   3) Range-Doppler haritası               (range_doppler)
//   4) CA-CFAR / OS-CFAR tespiti            (cfar)
//   5) Monte Carlo: Pd-SNR, gerçekleşen Pfa, süre   (evaluation)
//
// Çıktılar output/ klasörüne CSV olarak yazılır; scripts/plot.py bunlardan grafik üretir.
//
// Kullanım: radar_demo [--out DIR] [--trials N] [--no-mc]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>

#include "cfar.h"
#include "evaluation.h"
#include "pulse_compression.h"
#include "radar_params.h"
#include "range_doppler.h"
#include "signal_gen.h"

using namespace radar;

namespace {

struct SceneTarget {
  double range_m, velocity_mps, snr_db;
};

void print_detections(const char* label, const std::vector<Detection>& dets) {
  std::printf("\n%s: %zu tespit\n", label, dets.size());
  std::printf("  %10s %12s %9s\n", "menzil[m]", "hiz[m/s]", "SNR[dB]");
  for (const Detection& d : dets)
    std::printf("  %10.1f %12.2f %9.1f\n", d.range_m, d.velocity_mps, d.snr_db);
}

void write_detections(std::FILE* f, const char* type, const std::vector<Detection>& dets) {
  for (const Detection& d : dets)
    std::fprintf(f, "%s,%.2f,%.3f,%.2f,%zu,%zu\n", type, d.range_m, d.velocity_mps, d.snr_db,
                 d.range_bin, d.doppler_bin);
}

}  // namespace

int main(int argc, char** argv) {
  std::string out_dir = "output";
  int trials = 100;
  bool run_mc = true;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--out") && i + 1 < argc) out_dir = argv[++i];
    else if (!std::strcmp(argv[i], "--trials") && i + 1 < argc) trials = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--no-mc")) run_mc = false;
    else {
      std::printf("Kullanim: %s [--out DIR] [--trials N] [--no-mc]\n", argv[0]);
      return 1;
    }
  }
  std::filesystem::create_directories(out_dir);
  auto path = [&](const char* name) { return out_dir + "/" + name; };

  const RadarParams p;
  CfarParams cfar;  // varsayılan: guard 2x1, train 6x3, Pfa 1e-6

  std::printf("=== cfar-radar-pipeline ===\n");
  std::printf("Dalga boyu            : %.4f m\n", p.wavelength());
  std::printf("Menzil cozunurlugu    : %.2f m  (c / 2B)\n", p.range_resolution_m());
  std::printf("Menzil hucresi        : %.2f m  (c / 2fs)\n", p.range_bin_m());
  std::printf("Maks. menzil (pencere): %.0f m\n",
              p.range_bin_m() * static_cast<double>(p.num_samples));
  std::printf("Hiz cozunurlugu       : %.2f m/s\n", p.velocity_resolution_mps());
  std::printf("Belirsiz hiz siniri   : +-%.1f m/s\n", p.max_unambiguous_velocity_mps());
  std::printf("CFAR egitim hucresi   : %zu  (Pfa tasarim = %.0e)\n", cfar_num_training_cells(cfar),
              cfar.pfa);

  // ---- 1) Sahne -----------------------------------------------------------------------
  const SceneTarget scene[] = {
      {600.0, 30.0, 28.0},   // yakın, hızlı, güçlü
      {1100.0, -18.0, 20.0}, // uzaklaşan, orta
      {1600.0, 8.0, 22.0},   // uzak, yavaş
  };
  std::mt19937_64 rng(2024);
  CMatrix frame(p.num_pulses, p.num_samples);
  std::printf("\nGercek hedefler:\n  %10s %12s %9s\n", "menzil[m]", "hiz[m/s]", "SNR[dB]");
  for (const SceneTarget& s : scene) {
    Target t;
    t.range_m = s.range_m;
    t.velocity_mps = s.velocity_mps;
    t.amplitude = amplitude_for_integrated_snr(p, s.snr_db, 1.0);
    add_target(frame, p, t);
    std::printf("  %10.1f %12.2f %9.1f\n", s.range_m, s.velocity_mps, s.snr_db);
  }
  ClutterPatch clutter;  // 300-900 m arası sıfır Doppler çevresinde clutter şeridi
  clutter.start_range_m = 300.0;
  clutter.end_range_m = 900.0;
  clutter.scatterer_snr_db = -8.0;  // CNR ~35 dB (cok guclu clutter menzil yan lob tabani yaratir)
  add_clutter_patch(frame, p, clutter, 1.0, rng);
  add_noise(frame, 1.0, rng);

  // ---- 2-4) İşleme hattı --------------------------------------------------------------
  const PulseCompressor pc(make_lfm_chirp(p), p.num_samples);
  const CMatrix compressed = pc.compress(frame);
  const RMatrix rd_power = range_doppler_power(compressed, WindowType::Hann);
  const RMatrix rd_db = power_to_db(rd_power);
  const auto ca = cfar_detect(rd_power, p, cfar, CfarType::CellAveraging);
  const auto os = cfar_detect(rd_power, p, cfar, CfarType::OrderedStatistic);
  print_detections("CA-CFAR", ca);
  print_detections("OS-CFAR", os);

  write_csv(rd_db, path("rd_map_db.csv"));
  if (std::FILE* f = std::fopen(path("detections.csv").c_str(), "w")) {
    std::fprintf(f, "type,range_m,velocity_mps,snr_db,range_bin,doppler_bin\n");
    write_detections(f, "CA", ca);
    write_detections(f, "OS", os);
    std::fclose(f);
  }
  if (std::FILE* f = std::fopen(path("truth.csv").c_str(), "w")) {
    std::fprintf(f, "range_m,velocity_mps,snr_db\n");
    for (const SceneTarget& s : scene)
      std::fprintf(f, "%.2f,%.3f,%.1f\n", s.range_m, s.velocity_mps, s.snr_db);
    std::fclose(f);
  }
  if (std::FILE* f = std::fopen(path("meta.csv").c_str(), "w")) {
    std::fprintf(f, "range_bin_m,velocity_res_mps,num_pulses,num_samples,range_res_m\n");
    std::fprintf(f, "%.6f,%.6f,%zu,%zu,%.6f\n", p.range_bin_m(), p.velocity_resolution_mps(),
                 p.num_pulses, p.num_samples, p.range_resolution_m());
    std::fclose(f);
  }

  // ---- 5) Değerlendirme ---------------------------------------------------------------
  const Timing tm = measure_timing(p, cfar, 50, 1);
  const double total_ca = tm.compress_ms + tm.range_doppler_ms + tm.ca_cfar_ms;
  const double total_os = tm.compress_ms + tm.range_doppler_ms + tm.os_cfar_ms;
  std::printf("\nSure (ms/cerceve, ortalama 50 cerceve):\n");
  std::printf("  sinyal uretimi (simulasyon) : %7.2f\n", tm.generate_ms);
  std::printf("  darbe sikistirma            : %7.2f\n", tm.compress_ms);
  std::printf("  range-Doppler               : %7.2f\n", tm.range_doppler_ms);
  std::printf("  CA-CFAR                     : %7.2f\n", tm.ca_cfar_ms);
  std::printf("  OS-CFAR                     : %7.2f\n", tm.os_cfar_ms);
  std::printf("  isleme hatti toplam (CA/OS) : %7.2f / %.2f\n", total_ca, total_os);
  if (std::FILE* f = std::fopen(path("timing.csv").c_str(), "w")) {
    std::fprintf(f, "stage,ms_per_frame\n");
    std::fprintf(f, "signal_generation,%.3f\npulse_compression,%.3f\nrange_doppler,%.3f\n",
                 tm.generate_ms, tm.compress_ms, tm.range_doppler_ms);
    std::fprintf(f, "ca_cfar,%.3f\nos_cfar,%.3f\n", tm.ca_cfar_ms, tm.os_cfar_ms);
    std::fclose(f);
  }

  if (!run_mc) return 0;

  std::printf("\nMonte Carlo (%d deneme/nokta) -- birkac dakika surebilir...\n", trials);
  std::FILE* pd_file = std::fopen(path("pd_snr.csv").c_str(), "w");
  if (pd_file) std::fprintf(pd_file, "scenario,snr_db,pd_ca,pd_os\n");
  const Scenario scenarios[] = {Scenario::Homogeneous, Scenario::ClutterEdge,
                                Scenario::InterferingTarget};
  for (Scenario sc : scenarios) {
    std::printf("\nSenaryo: %s (Pfa tasarim %.0e)\n  %7s %8s %8s\n", scenario_name(sc), cfar.pfa,
                "SNR[dB]", "Pd CA", "Pd OS");
    for (int snr = 6; snr <= 24; snr += 2) {
      const PdPair r = measure_pd(p, cfar, sc, snr, trials, 1000 + static_cast<unsigned>(snr));
      std::printf("  %7d %8.3f %8.3f\n", snr, r.ca, r.os);
      if (pd_file) std::fprintf(pd_file, "%s,%d,%.4f,%.4f\n", scenario_name(sc), snr, r.ca, r.os);
    }
  }
  if (pd_file) std::fclose(pd_file);

  std::printf("\nGerceklesen Pfa (yalniz gurultu, 100 cerceve):\n  %10s %12s %12s\n", "tasarim",
              "CA", "OS");
  if (std::FILE* f = std::fopen(path("pfa.csv").c_str(), "w")) {
    std::fprintf(f, "design_pfa,pfa_ca,pfa_os\n");
    for (double design : {1e-2, 1e-3, 1e-4}) {
      CfarParams c = cfar;
      c.pfa = design;
      const PdPair r = measure_pfa(p, c, 100, 77);
      std::printf("  %10.0e %12.3e %12.3e\n", design, r.ca, r.os);
      std::fprintf(f, "%.0e,%.4e,%.4e\n", design, r.ca, r.os);
    }
    std::fclose(f);
  }
  std::printf("\nCSV dosyalari: %s/\n", out_dir.c_str());
  return 0;
}
