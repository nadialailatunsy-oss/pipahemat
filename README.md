# PipaHemat

Sistem deteksi kebocoran pipa air untuk kos dan rumah berbasis sensor aliran dan sensor level air.
Tugas Kelompok 4, mata kuliah Internet of Things.


## Deskripsi singkat

Kebocoran pipa di dinding, bawah lantai, atau sambungan tandon sering tidak terlihat. Pemilik baru tahu saat tagihan air membengkak, padahal meteran biasa hanya mencatat total pemakaian dan tidak membedakan air terpakai dari air bocor.

PipaHemat membandingkan air yang keluar dari tandon dengan air yang benar-benar lewat di pipa distribusi. Kalau air di tandon berkurang lebih banyak daripada yang terukur di pipa, selisihnya dianggap air yang hilang. Selisih yang bertahan cukup lama ditetapkan sebagai BOCOR, katup menutup otomatis, dan alert dikirim ke dashboard.

## Cara kerja

- **Edge 1 (ESP32 di pipa):** membaca debit dari sensor aliran (di Wokwi disimulasikan dengan potensiometer), menghaluskannya dengan moving average 5 pembacaan, lalu mengirimnya lewat MQTT.
- **Edge 2 (ESP32 di tandon):** membaca level air dengan HC-SR04, menghitung laju turun level dalam jendela 20 detik, lalu menjalankan algoritma neraca air. Edge 2 juga menggerakkan servo sebagai katup dan menerima perintah dari dashboard.
- **Dashboard (HP atau browser):** menampilkan debit, level tandon, status, dan estimasi air terbuang, serta punya tombol kontrol katup. Komunikasinya dua arah.

### Algoritma neraca air

1. `selisih = laju turun level - debit pipa`, dengan debit dirata-rata pada jendela waktu yang sama.
2. Selisih lebih dari 1,5 L/menit memulai timer. Selisih yang hilang lebih dari 15 detik membatalkan timer, sehingga lonjakan sesaat tidak dianggap bocor.
3. Timer mencapai durasi minimum (`LEAK_MIN_MS`, rancangan 5 menit, bisa dipersingkat untuk uji cepat): status BOCOR, katup menutup, alert dikirim, status ditahan sampai ada perintah BUKA.
4. Level yang naik mendadak (tandon diisi) mereset jendela data dan tidak dianggap bocor.

Status: `MENUNGGU_DATA`, `AMAN`, `WASPADA`, `BOCOR`.

## Dashboard

Dashboard dapat dibuka di: https://nadialailatunsy-oss.github.io/pipahemat/dashboard/dashboard.html

<img width="507" height="676" alt="image" src="https://github.com/user-attachments/assets/be7d60be-b98d-4f3e-9fd8-974235e0eb48" />

### Panduan pengguna

1. Buka alamat dashboard di atas lewat HP atau browser.
2. Pastikan kolom nama kelompok berisi `kelompok_4`, lalu klik **Hubungkan**. Tulisan di bawahnya berubah menjadi "Terhubung ke pipahemat/kelompok_4".
3. Baca kotak status besar di tengah halaman:
   - Abu-abu `MENUNGGU DATA`: sistem sedang mengumpulkan data awal.
   - Hijau `AMAN`: tidak ada kebocoran.
   - Oranye `WASPADA`: ada selisih air yang mencurigakan, belum cukup lama untuk dipastikan bocor.
   - Merah `BOCOR`: kebocoran terdeteksi, katup menutup otomatis.
4. Kotak angka menampilkan debit pipa, laju turun level, level tandon, selisih, estimasi air terbuang, dan posisi katup.
5. Tombol kontrol:
   - **Tutup Katup**: menutup katup secara manual.
   - **Buka Katup / Reset BOCOR**: membuka katup dan mengembalikan status setelah BOCOR.
   - **Reset hitungan**: mengosongkan estimasi air terbuang.
   - **Simulasi bocor 3 L/menit** dan **Hentikan simulasi**: khusus demo di Wokwi.
6. Saat BOCOR, peringatan masuk ke Log alert. Jika dashboard terbuka di HP dan izin notifikasi diberikan, HP bergetar dan menampilkan notifikasi.

## Komunikasi MQTT

Broker: `broker.hivemq.com` (publik).

| Topik | Arah | Isi |
|---|---|---|
| `pipahemat/kelompok_4/edge/edge1/telemetry` | Edge 1 ke broker | debit rata-rata |
| `pipahemat/kelompok_4/edge/edge2/telemetry` | Edge 2 ke broker | level, laju turun, selisih, status, katup |
| `pipahemat/kelompok_4/alerts` | Edge 2 ke dashboard | peringatan BOCOR |
| `pipahemat/kelompok_4/cmd/katup` | Dashboard ke Edge 2 | TUTUP, BUKA, RESET, SIM:n (demo) |

## Struktur repositori

- `edge1-pipa/` : firmware ESP32 di pipa (PlatformIO dan Wokwi)
- `edge2-tandon/` : firmware ESP32 di tandon (PlatformIO dan Wokwi)
- `dashboard/` : dashboard web

## Cara menjalankan di Wokwi

1. Buka folder `edge1-pipa` di VS Code, build dengan PlatformIO, lalu jalankan simulator Wokwi.
2. Buka folder `edge2-tandon` di jendela VS Code lain, build, lalu jalankan simulator.
3. Buka dashboard dan klik **Hubungkan**.

## Catatan

Pengujian dilakukan di simulator Wokwi. Kebocoran dipicu secara virtual (perintah `SIM`), dan durasi dihitung dalam waktu simulasi, sehingga air tidak benar-benar bocor. Untuk perangkat asli, gunakan sensor aliran YF-S201 dan atur `USE_POT` serta `SIM_LEVEL` ke 0.
