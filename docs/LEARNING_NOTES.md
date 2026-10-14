# Öğrenme Notları — cfar-radar-pipeline

Bu doküman, projeyi baştan sona okuyup mülakatta anlatabilecek düzeye gelmen için yazıldı.

## 1. Büyük resim

Radar bir darbe yollar, yankıyı dinler. Yankının **gecikmesi** menzili, darbeden darbeye **faz değişimi** hızı verir. Hat şu adımlardan oluşur:

```
 signal_gen        pulse_compression      range_doppler            cfar
 ham I/Q   ------> eşleşik filtre ------> slow-time FFT ---------> eşik + karar
 (darbe x örnek)   (menzil keskinleşir)   (hız ekseni oluşur)      (hedef listesi)
      |                                                                  |
  hedef+clutter+gürültü                                      menzil m, hız m/s, SNR dB
```

Veri yapısı: `Matrix<Complex>` — satır = darbe (**slow-time**), sütun = örnek (**fast-time** = menzil).

## 2. Hangi sırayla okunmalı?

1. `src/radar_params.h` — tüm temel denklemler tek yerde.
2. `src/matrix.h` — veri yapısı (kısa).
3. `src/signal_gen.h/.cpp` — fiziksel model: chirp, hedef, Doppler fazı, gürültü, clutter.
4. `src/fft.h/.cpp` + `tests/test_fft.cpp` — radix-2 FFT.
5. `src/pulse_compression.*` — eşleşik filtre.
6. `src/range_doppler.*` — pencereleme, Doppler FFT, eksenler.
7. `src/cfar.*` — CA ve OS-CFAR.
8. `src/evaluation.*` ve `src/main.cpp` — Monte Carlo ve uçtan uca demo.
9. Her modülün testi (`tests/`) — beklentiyi en net gösteren yer.

## 3. Modül modül mantık

### 3.1 `radar_params.h` — tasarım parametreleri

| Büyüklük | Formül | Bu projede |
|---|---|---|
| Dalga boyu | λ = c / fc | 3 cm (X-band, 10 GHz) |
| Menzil çözünürlüğü | ΔR = c / (2B) | 14.99 m (B = 10 MHz) |
| Menzil hücresi | c / (2 fs) | 7.49 m (fs = 20 MHz) |
| Doppler kayması | fd = 2v / λ | v = 30 m/s → 2 kHz |
| Hız çözünürlüğü | Δv = λ·PRF / (2N) | 2.34 m/s (N = 64) |
| Belirsiz hız | \|v\| < λ·PRF / 4 | ±75 m/s |

Çözünürlük (iki hedefi ayırma) ile hücre boyu (örnekleme aralığı) **farklı** şeylerdir: çözünürlüğü bant genişliği B, hücre boyunu örnekleme hızı fs belirler. fs = 2B seçerek menzilde 2x aşırı örnekleme yaparız.

### 3.2 `signal_gen` — sinyal modeli

* **LFM chirp**: s(t) = exp(jπk(t − Tp/2)²), k = B/Tp. Frekans −B/2 → +B/2 doğrusal süpürülür.
* **Hedef yankısı**: chirp'in τ = 2R/c kadar gecikmiş kopyası × genlik × taşıyıcı fazı exp(−j2π fc τ).
* **Doppler nereden gelir?** Hedef hareket edince R darbeden darbeye değişir; τ değişince taşıyıcı fazı döner. Faz adımı = 2π·fd/PRF. Yani slow-time'da `exp(j2π fd m/PRF)` bir sinüstür; FFT onun frekansını bulur.
* **Gürültü**: karmaşık Gauss, E[|n|²] = σ². Her bileşenin varyansı σ²/2.
* **Clutter**: menzil hücresi başına bir saçıcı, hızı ~N(0, σv²) → sıfır Doppler çevresinde şerit.
* **SNR tanımı**: `amplitude_for_integrated_snr` — bütünleştirme sonrası SNR = A²·L·N / σ² (L = chirp örnek sayısı = 200 ≈ 23 dB, N = darbe sayısı = 64 ≈ 18 dB). Toplam işleme kazancı ≈ 41 dB.

### 3.3 `fft` — radix-2 Cooley-Tukey

* DFT: X[k] = Σ x[n] e^{−j2πkn/N} → O(N²).
* Böl: çift/tek indeksler → X[k] = E[k] + W^k O[k], X[k+N/2] = E[k] − W^k O[k], W = e^{−j2π/N}. log₂N kez özyinele → O(N log N).
* Burada **özyinelemesiz**: önce bit-reversal permütasyonu, sonra `len = 2, 4, ..., N` aşamalarında "kelebek" (butterfly).
* Ters FFT: dönüş yönü ters + 1/N ölçekleme.
* `fftshift`: sıfır frekansı ortaya taşır → Doppler ekseni −PRF/2…+PRF/2.

### 3.4 `pulse_compression` — eşleşik filtre

* Uzun chirp çok enerji taşır ama yankılar zamanda yayılır. Alınan sinyal x, referans chirp s ile korele edilir: y[n] = Σ x[n+m]·conj(s[m]).
* Frekans bölgesinde: Y = X · conj(S) → FFT, çarp, IFFT (O(N log N)).
* Sonuç: tepe genişliği ≈ 1/B (→ ΔR = c/2B), tepe SNR kazancı = L.
* **Yan lob**: dikdörtgen zarflı LFM'nin sıkıştırma yan lobları −13 dB. Referansa **Hamming ağırlığı** uygulayınca ~−44 dB'ye iner. Bedel: ana lob genişler, ~1.3 dB SNR kaybı. Bunu sinyal üretiminde değil **alıcı filtresinde** yaparız (uyumsuz filtre).
* Dairesel korelasyon: pencere sonuna taşan yankılar başa sarar.

### 3.5 `range_doppler` — hız ekseni

* Her menzil hücresi için darbeler boyunca (sütun yönünde) pencere × FFT × fftshift.
* Doppler bini → hız: fd = (k − N/2)·PRF/N, v = fd·λ/2. Pozitif hız = yaklaşan.
* **Pencereleme**: dikdörtgen pencerede FFT yan lobları −13 dB; güçlü clutter zayıf hedefi örter. Hann: ~−31 dB (ve hızla düşen), ana lob 2x geniş, ~1.76 dB SNR kaybı. Hann ve Hamming'in farkı: Hann yan lobları daha hızlı düşer, Hamming ilk yan lobu daha çok bastırır.
* Scalloping: hedef iki Doppler bini arasındaysa tepe düşer (Hann'da en kötü ≈ −1.4 dB).

### 3.6 `cfar` — adaptif eşik

* Sabit eşik kullanılamaz: gürültü/clutter seviyesi yerel olarak değişir. CFAR eşiği, test hücresinin (CUT) çevresindeki **eğitim hücrelerinden** kestirdiği gürültüye göre kurar. CUT'un hemen yanındaki **guard hücreleri** hedefin kendi sızıntısının kestirimi bozmasını engeller.
* Gürültü gücü |x|² **üstel** dağılır. N bağımsız eğitim hücresi ve CA-CFAR için:

  Pfa = (1 + α/N)^(−N) → **α = N (Pfa^(−1/N) − 1)**, eşik = α · ortalama(eğitim)
* **OS-CFAR**: eşik = T · (k. sıralı istatistik), Pfa = Π_{i=0}^{k−1} (N−i)/(N−i+T). T'yi bisection ile çözeriz (kapalı form yok). k = 0.75N.
* **CA'nın zayıflığı**: eğitim penceresinde güçlü bir komşu varsa ortalama şişer → **maskeleme**, hedef kaçar (testte gösteriliyor). **OS'nin** avantajı: N−k kadar güçlü hücreye dayanıklı. Zayıflığı: sıralama maliyeti (~80x yavaş) ve eğitim hücrelerinin >%25'i clutter ise (uzamış clutter şeridi) eşik düşük kalır → çok sahte tespit (demoda zero-Doppler şeridinde görülüyor).
* Uygulama: CA için **integral görüntü** (summed-area table) ile dikdörtgen toplamı O(1); OS için `std::nth_element` (O(N)).
* Doppler ekseni periyodik olduğundan kenarlarda sarılır; menzil kenarlarında pencere sığmayan hücreler test edilmez.
* `local_maxima_only`: bir hedefin birkaç komşu hücreyi aşmasını tek tespite indirir (3×3 tepe).

### 3.7 `evaluation` — Monte Carlo

* **Pd** = hedefin doğru hücre ±1 komşuluğunda tespit edildiği deneme oranı. Hedef konumu/hızı/fazı her denemede rastgele (kesirli hücreler dahil → scalloping kaybı da ölçülür).
* **Pfa** = yalnızca gürültü; eşiği aşan hücre sayısı / test edilen hücre sayısı.
* CA ve OS **aynı çerçeveler** üzerinde çalışır (eşleştirilmiş karşılaştırma).
* Üç senaryo: homojen gürültü, clutter kenarı, hedefin eğitim penceresinde güçlü başka hedef.

## 4. Projeden çıkan önemli bulgular (kendi deneyimin)

1. **Gerçekleşen Pfa tasarımdan sapar.** Sıkıştırma (fs = 2B) komşu menzil hücrelerini, Hann penceresi komşu Doppler hücrelerini ilintilendirir → "bağımsız üstel hücre" varsayımı bozulur. `pfa.png`'de görülür.
2. **Çok güçlü clutter yan lob tabanı yaratır.** İlk demoda clutter-gürültü oranı ~50 dB idi; Hamming ağırlığıyla bile −44 dB'lik menzil yan lobları sıfır Doppler satırını her menzilde gürültünün ~15 dB üstüne çıkardı. Çözüm: gerçekçi CNR veya daha güçlü ağırlık / MTI.
3. **Durağan clutter'ı CFAR bastırmaz.** Sıfır Doppler şeridindeki güçlü hücreler hedef gibi görünür; bunun çözümü MTI/Doppler çentik filtresidir (gelecek işler).
4. **Tepe SNR ≠ ölçülen SNR.** Hann (−1.76 dB), Hamming ağırlığı (~−1.3 dB), scalloping ve CFAR kaybı yüzünden tespit edilen SNR, girilen SNR'ın birkaç dB altındadır.

## 5. Kendini test et (mülakat soruları)

1. Menzil çözünürlüğü neden `c/(2B)`? Chirp süresi uzarsa çözünürlük değişir mi, ne değişir?
2. Darbe sıkıştırma SNR'ı neden L (zaman-bant çarpımı kadar) artırır? Zaman-bant çarpımı nedir?
3. Radix-2 FFT'de bit-reversal neden gerekli? Karmaşıklığı neden O(N log N)?
4. Slow-time FFT neden hızı verir? PRF = 10 kHz, λ = 3 cm iken belirsiz hız sınırı nedir ve nereden gelir?
5. Hann ile dikdörtgen pencere arasındaki takas nedir? Ne zaman dikdörtgen tercih edersin?
6. CFAR'da guard hücreleri neden var? Olmasaydı ne olurdu?
7. CA-CFAR eşik çarpanı α = N(Pfa^(−1/N) − 1) formülünü nasıl türetirsin? (İpucu: üstel dağılım)
8. İki hedef birbirine yakınsa CA-CFAR neden zayıf olanı kaçırır? OS-CFAR bunu nasıl çözer, bedeli nedir?
9. Ölçtüğün Pfa tasarım Pfa'sından neden farklı çıktı? Hangi varsayım bozuluyor?
10. Bu hatta durağan clutter'ı nasıl bastırırsın? MTI iki-darbeli kanselleyicinin frekans yanıtı nedir ve yavaş hedeflere ne yapar?

### Cevap ipuçları

1. Ayırabilmek için iki yankının sıkıştırılmış tepelerinin ayrışması gerekir; tepe genişliği ≈ 1/B → gidiş-dönüş gecikme farkı 1/B → ΔR = c/(2B). Chirp süresi uzarsa çözünürlük aynı kalır ama enerji (ve SNR kazancı) artar.
2. Eşleşik filtre tüm enerjiyi tek tepede toplar; çıkış tepe gücü ∝ L², gürültü gücü ∝ L → SNR kazancı L (= B·Tp).
3. Özyinelemeli çift/tek ayırmanın sonunda elemanlar bit-ters sırada kalır. log₂N aşama × her aşamada N/2 kelebek.
4. Faz adımı 2π fd/PRF; fd, PRF'i aşınca katlanır: |fd| < PRF/2 → |v| < λ·PRF/4 = 75 m/s.
5. Yan lob bastırma ↔ ana lob genişliği ve SNR kaybı. Tek, izole hedef ve maksimum SNR/çözünürlük gerekirse dikdörtgen.
6. Hedefin enerjisi komşu hücrelere sızar; guard olmazsa eğitim ortalaması hedefin kendisiyle şişer, eşik yükselir, hedef kendi eşiğini aşamaz (self-masking).
7. Gürültü gücü üstel(μ) → P(x > αμ̂) ; μ̂ N hücrenin ortalaması (Gamma) → MGF ile (1+α/N)^(−N).
8. Yakın güçlü hedef ortalamayı şişirir (maskeleme). OS-CFAR k. sıralı istatistik kullandığından N−k güçlü hücreye dayanır; bedeli sıralama maliyeti, homojen gürültüde küçük SNR kaybı ve uzamış clutter'da (>%25) zayıflık.
9. Bağımsız üstel hücre varsayımı: sıkıştırma ve pencereleme komşu hücreleri ilintilendirir; ayrıca kenar etkileri ve yerel gürültü kestirim varyansı.
10. H(z) = 1 − z⁻¹ → |H(f)| = 2|sin(π f/PRF)|; sıfır Doppler'de sıfır, ama düşük hızlı hedefleri de (kör hız) bastırır; PRF katlarında da kör hızlar oluşur.
