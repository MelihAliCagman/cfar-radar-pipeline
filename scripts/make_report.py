#!/usr/bin/env python3
"""make_report.py — proje kodu, testler ve sonuclar hakkinda ayrintili PDF uretir.

Kullanim:  python scripts/make_report.py   (repo kokunden)
Girdi   :  output/*.csv (radar_demo), docs/images/*.png, src/, tests/, ctest ciktisi
Cikti   :  reports/cfar_radar_pipeline_report.pdf   (reports/ .gitignore'dadir)
Gerekli :  reportlab
"""
import csv
import subprocess
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_JUSTIFY
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import cm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (Image, KeepTogether, PageBreak, Paragraph, Preformatted,
                                SimpleDocTemplate, Spacer, Table, TableStyle)

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "output"
IMG = ROOT / "docs" / "images"
PDF = ROOT / "reports" / "cfar_radar_pipeline_report.pdf"
PDF.parent.mkdir(exist_ok=True)

FONTS = Path("C:/Windows/Fonts")
pdfmetrics.registerFont(TTFont("Body", str(FONTS / "arial.ttf")))
pdfmetrics.registerFont(TTFont("Body-Bold", str(FONTS / "arialbd.ttf")))
pdfmetrics.registerFont(TTFont("Body-Italic", str(FONTS / "ariali.ttf")))
pdfmetrics.registerFont(TTFont("Mono", str(FONTS / "consola.ttf")))
pdfmetrics.registerFontFamily("Body", normal="Body", bold="Body-Bold", italic="Body-Italic",
                              boldItalic="Body-Bold")

ss = getSampleStyleSheet()
BODY = ParagraphStyle("body", parent=ss["BodyText"], fontName="Body", fontSize=10, leading=14.5,
                      alignment=TA_JUSTIFY, spaceAfter=6)
H1 = ParagraphStyle("h1", parent=BODY, fontName="Body-Bold", fontSize=17, leading=21,
                    spaceBefore=6, spaceAfter=10, alignment=0, textColor=colors.HexColor("#14304f"))
H2 = ParagraphStyle("h2", parent=H1, fontSize=12.5, leading=16, spaceBefore=10, spaceAfter=5)
CAPTION = ParagraphStyle("cap", parent=BODY, fontSize=8.5, leading=11, textColor=colors.grey,
                         alignment=1)
CODE = ParagraphStyle("code", fontName="Mono", fontSize=6.9, leading=8.4,
                      backColor=colors.HexColor("#f4f6f8"), borderPadding=4)
TITLE = ParagraphStyle("title", parent=H1, fontSize=26, leading=32, alignment=0, spaceAfter=12)
BULLET = ParagraphStyle("bullet", parent=BODY, leftIndent=14, bulletIndent=2, spaceAfter=2)


def P(text, style=BODY):
    return Paragraph(text, style)


def bullets(items):
    return [Paragraph(t, BULLET, bulletText="•") for t in items]


def wrap_lines(text, width=122):
    out = []
    for line in text.expandtabs(2).splitlines():
        while len(line) > width:
            cut = line.rfind(" ", 0, width)
            cut = cut if cut > width // 2 else width
            out.append(line[:cut])
            line = "      " + line[cut:].lstrip()
        out.append(line)
    return out


def code_block(text, chunk=70):
    lines = wrap_lines(text)
    return [Preformatted("\n".join(lines[i:i + chunk]), CODE) for i in range(0, len(lines), chunk)]


def table(rows, col_widths=None, header=True, font=8.5):
    t = Table(rows, colWidths=col_widths, repeatRows=1 if header else 0)
    style = [("FONT", (0, 0), (-1, -1), "Body", font),
             ("GRID", (0, 0), (-1, -1), 0.4, colors.HexColor("#c5ccd4")),
             ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
             ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#f4f6f8")])]
    if header:
        style += [("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#14304f")),
                  ("TEXTCOLOR", (0, 0), (-1, 0), colors.white), ("FONT", (0, 0), (-1, 0), "Body-Bold", font)]
    t.setStyle(TableStyle(style))
    return t


def figure(name, width_cm, caption):
    path = IMG / name
    if not path.exists():
        return []
    from PIL import Image as PILImage
    w, h = PILImage.open(path).size
    img = Image(str(path), width=width_cm * cm, height=width_cm * cm * h / w)
    return [KeepTogether([img, P(caption, CAPTION)])]


def read_csv(name):
    with open(OUT / name, newline="") as f:
        return list(csv.DictReader(f))


def page_footer(canvas, doc):
    canvas.saveState()
    canvas.setFont("Body", 8)
    canvas.setFillColor(colors.grey)
    canvas.drawString(2 * cm, 1.2 * cm, "cfar-radar-pipeline — Proje Raporu")
    canvas.drawRightString(A4[0] - 2 * cm, 1.2 * cm, f"Sayfa {doc.page}")
    canvas.restoreState()


story = []

# ------------------------------------------------------------------ Kapak ------------------
story += [Spacer(1, 4 * cm), P("cfar-radar-pipeline", TITLE),
          P("Simüle radar verisinde darbe sıkıştırma, range-Doppler işleme ve CFAR hedef tespiti",
            ParagraphStyle("sub", parent=BODY, fontSize=13, leading=18, alignment=0)),
          Spacer(1, 0.6 * cm),
          P("Proje raporu: mimari, kod, testler ve ölçülen sonuçlar", BODY),
          Spacer(1, 6 * cm),
          P("C++17 · CMake · radix-2 FFT · CA-CFAR / OS-CFAR · Monte Carlo değerlendirme", BODY),
          P("Depo: github.com/MelihAliCagman/cfar-radar-pipeline", BODY), PageBreak()]

# ------------------------------------------------------------------ 1. Genel bakis -----------
story += [P("1. Projenin amacı ve genel yapı", H1),
          P("Bu proje, bir radarın alıcısından gelen ham I/Q verisini (simüle edilmiş) menzil ve hız bilgisi "
            "içeren bir hedef listesine dönüştüren tam bir sinyal işleme hattıdır. Tüm algoritmalar "
            "(FFT dahil) harici kütüphane kullanılmadan C++17 ile yazılmıştır; amaç hem çalışan bir sistem "
            "hem de her adımın fiziksel/matematiksel gerekçesinin anlaşılabilir olmasıdır."),
          P("Hat şu adımlardan oluşur:"),
          Preformatted(
              "  signal_gen          pulse_compression       range_doppler            cfar\n"
              "  ham I/Q matrisi --> eşleşik filtre -------> slow-time FFT ---------> CA / OS eşik\n"
              "  (darbe x örnek)     (menzil keskinleşir)    (hız ekseni oluşur)      (hedef listesi)\n"
              "       ^                                                                    |\n"
              "  hedefler + clutter + gürültü                          menzil [m], hız [m/s], SNR [dB]\n"
              "\n"
              "  evaluation: aynı hat üzerinde Monte Carlo ile Pd, gerçekleşen Pfa ve süre ölçümü",
              ParagraphStyle("diag", parent=CODE, fontSize=8, leading=10.5)),
          Spacer(1, 6)]

meta = read_csv("meta.csv")[0]
story += [P("Radar parametreleri", H2),
          table([["Parametre", "Değer", "Türetilen büyüklük", "Değer"],
                 ["Taşıyıcı frekans", "10 GHz (X-band)", "Dalga boyu λ = c/fc", "3.0 cm"],
                 ["Chirp bant genişliği B", "10 MHz", "Menzil çözünürlüğü c/(2B)", f"{float(meta['range_res_m']):.2f} m"],
                 ["Örnekleme frekansı fs", "20 MHz", "Menzil hücresi c/(2fs)", f"{float(meta['range_bin_m']):.2f} m"],
                 ["Darbe genişliği Tp", "10 µs (200 örnek)", "Hız çözünürlüğü λ·PRF/(2N)", f"{float(meta['velocity_res_mps']):.2f} m/s"],
                 ["PRF", "10 kHz", "Belirsiz hız ±λ·PRF/4", "±75 m/s"],
                 ["Darbe sayısı N", "64", "Sıkıştırma kazancı", "200 (≈ 23 dB)"],
                 ["Darbe başına örnek", "512", "Darbeler arası kazanç", "64 (≈ 18 dB)"]],
                col_widths=[4 * cm, 3.6 * cm, 5.2 * cm, 3.4 * cm]),
          Spacer(1, 8),
          P("Depo yapısı", H2)]
story += code_block(
    "cfar-radar-pipeline/\n"
    "|-- CMakeLists.txt            # statik kütüphane 'radar', çalıştırılabilir 'radar_demo', testler\n"
    "|-- src/\n"
    "|   |-- radar_params.h        # tasarım parametreleri + türetilen büyüklükler\n"
    "|   |-- matrix.h              # satır-öncelikli 2B dizi (darbe x örnek)\n"
    "|   |-- signal_gen.{h,cpp}    # LFM chirp, hedef, gürültü, clutter\n"
    "|   |-- fft.{h,cpp}           # radix-2 FFT/IFFT, naif DFT, fftshift\n"
    "|   |-- pulse_compression.*   # eşleşik filtre (FFT tabanlı korelasyon)\n"
    "|   |-- range_doppler.*       # pencereleme, Doppler FFT, eksenler, CSV\n"
    "|   |-- cfar.*                # CA-CFAR ve OS-CFAR (2B)\n"
    "|   |-- evaluation.*          # Monte Carlo: Pd, Pfa, süre\n"
    "|   `-- main.cpp              # uçtan uca demo\n"
    "|-- tests/                    # 5 test çalıştırılabilir dosyası (CTest)\n"
    "|-- scripts/plot.py           # grafikler\n"
    "`-- docs/                     # LEARNING_NOTES.md, images/")
story.append(PageBreak())

# ------------------------------------------------------------------ 2. Moduller --------------
story += [P("2. Modüllerin mantığı ve denklemler", H1),
          P("2.1 Sinyal modeli (signal_gen)", H2),
          P("Her darbede radar bir doğrusal frekans modülasyonlu (LFM) chirp gönderir: "
            "s(t) = exp(jπk(t − Tp/2)²), k = B/Tp. Anlık frekans −B/2'den +B/2'ye doğrusal süpürülür. "
            "R menzilindeki hedef, chirp'in τ = 2R/c kadar gecikmiş bir kopyasını A genliğiyle ve "
            "exp(−j2π·fc·τ) taşıyıcı fazıyla geri yansıtır."),
          P("<b>Doppler</b>: hedef hareket ederse R darbeden darbeye değişir, faz her darbede "
            "2π·fd/PRF kadar döner (fd = 2v/λ). Slow-time boyunca bu faz bir sinüs oluşturur; "
            "FFT'sinin tepesi hızı verir. Gürültü karmaşık Gauss'tur: E[|n|²] = σ²; clutter ise menzil "
            "hücresi başına bir saçıcıdan oluşur (Rayleigh genlik, rastgele faz, hız ~ N(0, σv²))."),
          P("<b>SNR tanımı</b>: bütünleştirme sonrası SNR = A²·L·N/σ² — sıkıştırma kazancı L = 200 "
            "(≈ 23 dB) ve tutarlı darbe toplama kazancı N = 64 (≈ 18 dB); toplam ≈ 41 dB. Bu yüzden "
            "ham veride gürültünün altında kalan bir hedef (giriş SNR ≈ −16 dB) 25 dB'lik bir tepe verebilir."),
          P("2.2 Radix-2 FFT (fft)", H2),
          P("N noktalı DFT X[k] = Σ x[n]e^(−j2πkn/N) doğrudan O(N²)'dir. Çift/tek indeksleri ayırıp "
            "X[k] = E[k] + W^k·O[k], X[k+N/2] = E[k] − W^k·O[k] (W = e^(−j2π/N)) ile birleştirmek ve bunu "
            "log₂N kez uygulamak O(N log N) verir. Uygulama özyinelemesizdir: önce bit-reversal permütasyonu, "
            "ardından len = 2, 4, …, N aşamalarında kelebek işlemleri. Ters FFT dönüş yönünü çevirir ve 1/N ile ölçekler. "
            "Doğruluk, naif DFT ile karşılaştırılarak test edilir."),
          P("2.3 Darbe sıkıştırma (pulse_compression)", H2),
          P("Alınan sinyal x, gönderilen chirp s ile korele edilir: y[n] = Σ x[n+m]·conj(s[m]). Frekans bölgesinde "
            "Y[k] = X[k]·conj(S[k]), yani FFT → çarp → IFFT. Çıkışta tepe genişliği ≈ 1/B (menzil çözünürlüğü "
            "ΔR = c/2B, chirp süresinden bağımsız) ve SNR kazancı L = B·Tp olur. Dikdörtgen zarflı LFM'nin "
            "sıkıştırma yan lobları −13 dB'dir; referansa Hamming ağırlığı uygulayınca yan loblar yaklaşık −44 dB'ye "
            "iner (bedel: ana lob genişler, ≈ 1.3 dB SNR kaybı). Referans spektrumu bir kez hesaplanır ve saklanır."),
          P("2.4 Range-Doppler haritası (range_doppler)", H2),
          P("Sıkıştırılmış matriste her menzil hücresi için darbeler boyunca pencere × FFT × fftshift uygulanır. "
            "Doppler bini k → hız: fd = (k − N/2)·PRF/N, v = fd·λ/2 (pozitif = yaklaşan). Hann penceresi FFT yan "
            "loblarını −13 dB'den ≈ −31 dB'ye indirir; bedeli ana lobun genişlemesi ve ≈ 1.76 dB SNR kaybıdır. "
            "Hedef iki bin arasındaysa ek 'scalloping' kaybı (Hann'da en çok ≈ 1.4 dB) oluşur."),
          P("2.5 CFAR (cfar)", H2),
          P("Gürültü seviyesi yerel olarak değiştiğinden sabit eşik kullanılamaz. CFAR, test hücresinin (CUT) "
            "çevresindeki N eğitim hücresinden gürültüyü kestirir; CUT'a bitişik guard hücreleri hedef "
            "sızıntısının kestirimi bozmasını engeller. Varsayılan pencere: guard 2×1 (menzil×Doppler), "
            "eğitim 6×3 → N = 138 hücre."),
          P("<b>CA-CFAR</b>: eşik = α·ortalama(eğitim), α = N(Pfa^(−1/N) − 1). Bu, üstel dağılımlı gürültü gücü ve "
            "N bağımsız hücre için Pfa = (1 + α/N)^(−N) denkleminin çözümüdür."),
          P("<b>OS-CFAR</b>: eşik = T·(k. sıralı istatistik), k = 0.75N; T, Pfa = Π(i=0..k−1) (N−i)/(N−i+T) "
            "denkleminden bisection ile bulunur."),
          P("<b>Uygulama</b>: CA için integral görüntü (summed-area table) ile herhangi bir dikdörtgenin toplamı O(1); "
            "OS için std::nth_element. Doppler ekseni periyodik olduğu için kenarlarda sarılır. "
            "Eşiği aşan hücreler arasından yalnızca 3×3 komşulukta yerel maksimum olanlar tespit sayılır."),
          P("2.6 Değerlendirme (evaluation)", H2),
          P("Pd: hedefin doğru hücrenin ±1 komşuluğunda tespit edildiği denemelerin oranı. Hedefin menzili (kesirli "
            "hücre dahil), hızı (kesirli Doppler binine dahil) ve fazı her denemede rastgeledir; dolayısıyla "
            "scalloping kayıpları ölçüme dahildir. Pfa: yalnızca gürültü içeren çerçevelerde eşiği aşan hücre sayısı / "
            "test edilen hücre sayısı. CA ve OS aynı çerçeveler üzerinde koşturulur (eşleştirilmiş karşılaştırma). "
            "Üç senaryo: (a) homojen gürültü, (b) hedefin eğitim penceresinin yarısı clutter içinde (clutter kenarı), "
            "(c) hedefin eğitim penceresinde 15 dB daha güçlü ikinci bir hedef."),
          PageBreak()]

# ------------------------------------------------------------------ 3. Testler ----------------
story += [P("3. Testler", H1),
          P("Test çerçevesi dışarıdan bağımlılık içermez (tests/test_util.h: CHECK ve CHECK_NEAR makroları); her test "
            "ayrı bir çalıştırılabilir dosyadır ve CTest bunları çalıştırır.")]
tests = [["Test", "Neyi doğrular"],
         ["test_signal_gen", "Çözünürlük/hücre hesapları; chirp birim genliği; yankı başlangıcı ve süresi (kesirli gecikme); "
                             "Doppler faz dönüşü = 2π·fd/PRF; gürültü gücü; SNR yardımcısı"],
         ["test_fft", "Bilinen sinüs → doğru binde tepe (yükseklik N); FFT = naif DFT (N = 2…256); ifft(fft(x)) = x; "
                      "Parseval; fftshift; 2'nin kuvveti olmayan uzunluk reddi"],
         ["test_pulse_compression", "Tepe doğru örnekte ve yüksekliği A·L; kesirli gecikme; −5 dB giriş SNR'ında bile "
                                    "tepe bulunur; 4 hücre arayla iki hedefin ayrışması; Hamming yan lob bastırma"],
         ["test_range_doppler", "Üç farklı hızda hedef beklenen (Doppler, menzil) hücresinde; eksen dönüşümleri; "
                                "Hann yan loblarını bastırır; pencere şekli; dB dönüşümü"],
         ["test_cfar", "α formüllerinin Pfa'ya geri yerleştirilmesi; üstel gürültüde gerçekleşen Pfa tasarımın %20'si içinde "
                       "(CA ve OS); CA-CFAR maskeleme (güçlü komşu hedefi kaçırır, OS bulur); uçtan uca tespit"]]
story.append(table([[P(c, ParagraphStyle("c", parent=BODY, fontSize=8.5, leading=11, alignment=0)) for c in r]
                    for r in tests], col_widths=[3.8 * cm, 12.4 * cm]))
story.append(Spacer(1, 8))

ctest = subprocess.run(["ctest", "--test-dir", str(ROOT / "build"), "--output-on-failure"],
                       capture_output=True, text=True)
story += [P("CTest çıktısı (bu raporun üretildiği an)", H2)] + code_block(ctest.stdout.strip())
story.append(PageBreak())

# ------------------------------------------------------------------ 4. Sonuclar ---------------
story += [P("4. Sonuçlar", H1), P("4.1 Demo sahnesi ve range-Doppler haritası", H2),
          P("Sahne: üç hedef — (600 m, +30 m/s, 28 dB), (1100 m, −18 m/s, 20 dB), (1600 m, +8 m/s, 22 dB) — ve "
            "300–900 m arasında sıfır Doppler çevresinde bir clutter şeridi (saçıcı başına −8 dB, clutter-gürültü oranı "
            "≈ 35 dB). Her iki dedektör de üç hedefi de doğru menzil/hızda buldu (menzil hatası < 1 hücre, hız hatası < 1 "
            "Doppler bini). Sıfır Doppler şeridindeki ek tespitler gerçek durağan clutter tepeleridir.")]
story += figure("range_doppler.png", 15.5, "Şekil 1 — Range-Doppler haritası; beyaz daireler gerçek hedefler, "
                "mavi daireler CA-CFAR, turuncu çarpılar OS-CFAR tespitleri.")

det = read_csv("detections.csv")
truth = read_csv("truth.csv")
rows = [["Algoritma", "Menzil [m]", "Hız [m/s]", "SNR [dB]"]]
for t in truth:
    for typ in ("CA", "OS"):
        best = min((d for d in det if d["type"] == typ and abs(float(d["velocity_mps"]) - float(t["velocity_mps"])) < 3),
                   key=lambda d: abs(float(d["range_m"]) - float(t["range_m"])), default=None)
        if best:
            rows.append([f"{typ}  (gerçek: {float(t['range_m']):.0f} m, {float(t['velocity_mps']):+.0f} m/s, {t['snr_db']} dB)",
                         f"{float(best['range_m']):.1f}", f"{float(best['velocity_mps']):+.2f}", f"{float(best['snr_db']):.1f}"])
story += [P("Gerçek hedeflerle eşleşen tespitler", H2), table(rows, col_widths=[8.4 * cm, 2.6 * cm, 2.6 * cm, 2.6 * cm]),
          Spacer(1, 4),
          P("Ölçülen SNR, girilen SNR'dan birkaç dB düşüktür; bunun nedenleri Hann penceresi (≈ 1.76 dB), "
            "Hamming ağırlıklı eşleşik filtre (≈ 1.3 dB), scalloping ve CFAR kestirim kaybıdır.")]

story += [P("4.2 Pd – SNR", H2),
          P("Her SNR noktası için aynı rastgele çerçevelerde CA ve OS-CFAR çalıştırılmıştır (tasarım Pfa = 10⁻⁶).")]
story += figure("pd_vs_snr.png", 16.5, "Şekil 2 — Üç senaryoda Pd–SNR eğrileri.")

pd_rows = read_csv("pd_snr.csv") if (OUT / "pd_snr.csv").exists() else []
for sc, label in (("homogeneous", "Homojen gürültü"), ("clutter_edge", "Clutter kenarı"),
                  ("interfering_target", "Yakın güçlü hedef")):
    sel = [r for r in pd_rows if r["scenario"] == sc]
    if not sel:
        continue
    story += [P(label, H2), table([["SNR [dB]"] + [r["snr_db"] for r in sel],
                                   ["Pd CA-CFAR"] + [f"{float(r['pd_ca']):.2f}" for r in sel],
                                   ["Pd OS-CFAR"] + [f"{float(r['pd_os']):.2f}" for r in sel]],
                                  col_widths=[2.6 * cm] + [1.5 * cm] * len(sel))]

story += [Spacer(1, 6), P("Yorum", H2)] + bullets([
    "<b>Homojen gürültüde</b> CA ve OS pratikte aynıdır; Pd = 0.5 civarı ≈ 15–16 dB'dedir. Bu nokta ideal "
    "bağımsız-hücre değerinin birkaç dB sağındadır; fark pencere kaybı, scalloping ve CFAR kayıplarından gelir.",
    "<b>Yakın güçlü hedef senaryosunda</b> CA-CFAR hedefi hemen hiç bulamaz: eğitim penceresindeki 15 dB daha güçlü "
    "komşu ortalamayı şişirir, eşik hedefin üstüne çıkar (maskeleme). OS-CFAR, k. sıralı istatistik tek bir güçlü "
    "hücreden etkilenmediği için yüksek SNR'larda Pd ≈ 1'e ulaşır.",
    "<b>Clutter kenarında</b> iki dedektör benzer davranır; OS yüksek SNR'da hafif öndedir. CA yüksek SNR'da "
    "0.95–0.97 civarında kalırken OS 20 dB'den itibaren 1.0'a ulaşır."])

pfa = read_csv("pfa.csv") if (OUT / "pfa.csv").exists() else []
story += [P("4.3 Gerçekleşen Pfa", H2),
          P("Yalnızca gürültü içeren 100 çerçevede (her biri 64 × ~496 test hücresi) eşiği aşan hücre oranı:")]
story += figure("pfa.png", 9, "Şekil 3 — Tasarım Pfa'sı ve gerçekleşen Pfa.")
if pfa:
    story += [table([["Tasarım Pfa", "CA-CFAR (ölçülen)", "OS-CFAR (ölçülen)", "Oran (CA)"]] +
                    [[r["design_pfa"], f"{float(r['pfa_ca']):.2e}", f"{float(r['pfa_os']):.2e}",
                      f"{float(r['pfa_ca']) / float(r['design_pfa']):.2f}x"] for r in pfa],
                    col_widths=[3.5 * cm, 4 * cm, 4 * cm, 3 * cm]),
              Spacer(1, 4),
              P("Ölçülen Pfa tasarım değerinin yaklaşık 1.3–2.5 katıdır. Sebep: tasarım formülleri bağımsız üstel "
                "hücreleri varsayar; oysa fs = 2B ile örneklenen sıkıştırılmış gürültü komşu menzil hücrelerinde, "
                "Hann penceresi komşu Doppler hücrelerinde ilintilidir. İlintili hücrelerde eğitim ortalamasının varyansı "
                "büyür ve eşik daha sık aşılır. Düşük Pfa'larda sapma büyür (10⁻⁴'te ≈ 2.5x).")]

tm = read_csv("timing.csv")
story += [P("4.4 İşlem süresi", H2)]
story += figure("timing.png", 11, "Şekil 4 — Aşama başına ortalama süre (ms/çerçeve).")
story += [table([["Aşama", "ms / çerçeve"]] + [[r["stage"].replace("_", " "), f"{float(r['ms_per_frame']):.2f}"] for r in tm],
                col_widths=[6 * cm, 3 * cm]), Spacer(1, 4)]
t = {r["stage"]: float(r["ms_per_frame"]) for r in tm}
chain_ca = t["pulse_compression"] + t["range_doppler"] + t["ca_cfar"]
story += [P(f"İşleme hattı (sıkıştırma + range-Doppler + CA-CFAR) ≈ {chain_ca:.1f} ms/çerçeve (tek çekirdek, "
            f"Release/-O3). OS-CFAR'ın sıralama maliyeti nedeniyle CA'dan ≈ {t['os_cfar'] / t['ca_cfar']:.0f} kat "
            "yavaş olduğuna dikkat: CA integral görüntüyle O(1)/hücre, OS her hücrede N = 138 elemanlı nth_element çalıştırır. "
            "Süreler makinenin o anki yüküne duyarlıdır; mutlak değerlerden çok oranlar anlamlıdır.")]
story.append(PageBreak())

# ------------------------------------------------------------------ 5. Bulgular ---------------
story += [P("5. Geliştirme sırasında karşılaşılan olaylar ve çıkarılan dersler", H1)] + bullets([
    "<b>Clutter yan lob tabanı.</b> İlk demoda clutter-gürültü oranı ≈ 50 dB'ydi. Menzil yan lobları sıfır Doppler "
    "satırında her menzilde (clutter'dan 3 km uzakta bile) gürültünün ≈ 15 dB üstünde bir 'taban' oluşturdu ve "
    "zayıf hedef kayboldu. Tek bir hedefle yapılan izole deney yan lobların (Hamming ile) −44 dB olduğunu gösterdi: "
    "yani 80 saçıcının toplam gücü bu tabanı yükseltiyordu. Çözüm: eşleşik filtreye Hamming ağırlığı eklemek "
    "ve demodaki clutter'ı gerçekçi seviyeye (CNR ≈ 35 dB) indirmek.",
    "<b>CFAR durağan clutter'ı bastırmaz.</b> Sıfır Doppler şeridindeki güçlü hücreler dedektöre hedef gibi görünür; "
    "OS-CFAR bu şeritte CA'dan daha fazla tespit üretir, çünkü eğitim hücrelerinin >%25'i clutter ise 75. yüzdelik "
    "dilim gürültü seviyesinde kalır. Çözüm CFAR değil MTI/Doppler çentik filtresidir (gelecek iş).",
    "<b>Gerçekleşen Pfa tasarımdan sapar</b> (bkz. 4.3); ilintili hücreler ölçülerek gösterildi.",
    "<b>Platform notu.</b> Windows/MSYS2 ortamında bazı kabuklardan derleyici sessizce hata verdi; derleme "
    "PowerShell'den sorunsuz çalıştı. Kodun kendisiyle ilgili değildi."])

story += [P("6. Kısıtlar ve gelecek işler", H1)] + bullets([
    "Basitleştirilmiş fizik: stop-and-hop, darbeler arası menzil göçü ve anten/yayılım kayıpları yok; tek kanal.",
    "Dairesel korelasyon nedeniyle pencerenin son ~200 örneğine düşen hedefler sarar/kesilir; değerlendirme bu bölgeden kaçınır.",
    "Hedef parametreleri ölçüm için hücre ızgarası üzerinde kestirilir; alt-hücre interpolasyonu yok.",
    "Gelecek: MTI/Doppler çentik filtresi, ardışık çerçevelerde Kalman filtresiyle takip, SIMD/OpenMP ile OS-CFAR hızlandırma, "
    "FFTW karşılaştırması, ölçülen Pfa'yı düzeltmek için kalibre edilmiş eşik çarpanı."])
story.append(PageBreak())

# ------------------------------------------------------------------ Ek: kaynak kodlar ---------
story += [P("Ek A — Kaynak kodlar", H1),
          P("Aşağıda hattın çekirdek modüllerinin tam kaynakları yer alır (açıklamalar kodun içindeki yorumlarda).")]
for rel in ["src/radar_params.h", "src/matrix.h", "src/signal_gen.h", "src/signal_gen.cpp", "src/fft.h", "src/fft.cpp",
            "src/pulse_compression.h", "src/pulse_compression.cpp", "src/range_doppler.h", "src/range_doppler.cpp",
            "src/cfar.h", "src/cfar.cpp", "src/evaluation.h", "src/evaluation.cpp", "src/main.cpp"]:
    story += [P(rel, H2)] + code_block((ROOT / rel).read_text(encoding="utf-8"))
story.append(PageBreak())
story += [P("Ek B — Test kaynakları", H1)]
for rel in ["tests/test_util.h", "tests/test_signal_gen.cpp", "tests/test_fft.cpp", "tests/test_pulse_compression.cpp",
            "tests/test_range_doppler.cpp", "tests/test_cfar.cpp"]:
    story += [P(rel, H2)] + code_block((ROOT / rel).read_text(encoding="utf-8"))

doc = SimpleDocTemplate(str(PDF), pagesize=A4, leftMargin=2 * cm, rightMargin=2 * cm, topMargin=2 * cm,
                        bottomMargin=2 * cm, title="cfar-radar-pipeline — Proje Raporu",
                        author="Melih Ali Çağman")
doc.build(story, onFirstPage=page_footer, onLaterPages=page_footer)
print("Yazildi:", PDF)
