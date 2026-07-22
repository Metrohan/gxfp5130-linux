# Goodix GXFP5130 Linux Parmak İzi Sürücüsü

Huawei MateBook dizüstü bilgisayarlardaki **Goodix GXFP5130** parmak izi sensörü
için eksiksiz Linux desteği — çekirdek modülü, kullanıcı alanı araçları, libfprint
entegrasyonu ve PAM kurulumu bir arada.

**[English](README.md)** | Türkçe

[![Kernel Patch: İncelemede](https://img.shields.io/badge/Kernel%20Patch-%C4%B0ncelemede-yellow.svg)](https://lore.kernel.org/linux-kernel/20260718080917.21893-1-metehangnen@gmail.com/)

> Huawei MateBook D16 2024 (MCLF-XX) üzerinde çalışıyor.
> Farklı bir modelde çalıştırdıysanız lütfen
> [uyumluluk raporu açın](https://github.com/Metrohan/gxfp5130-linux/issues/new?template=compatibility_report.yml).

> **Emek:** Orijinal çekirdek sürücüsü, `gxfpmoc` kullanıcı alanı kütüphanesi ve
> libfprint SIGFM çatalı [**Void755**](https://github.com/Void755) tarafından
> oluşturulmuştur. Bu repo o çalışmayı dağıtım için paketler (Arch paketi, PAM
> entegrasyonu, mainline çekirdeğe gönderim) — tam kaynak anlık görüntüleri için
> aşağıdaki [Kaynak Kodun Kökeni](#kaynak-kodun-kökeni) bölümüne bakın.

---

## Upstream durumu

Çekirdek sürücüsü, mainline'a dahil edilmek üzere Linux kernel mailing
list'ine gönderildi:

**[[PATCH 0/4] drivers/misc: add Goodix GXFP5130 eSPI fingerprint sensor driver](https://lore.kernel.org/linux-kernel/20260718080917.21893-1-metehangnen@gmail.com/)**

Kabul edildiğinde modül, mainline çekirdekle birlikte gelecek ve
desteklenen dağıtımlarda DKMS kurulumuna gerek kalmayacak.

---

## Ne işe yarar?

GXFP5130, USB veya PCIe üzerinde değil — Gömülü Denetleyici'nin (EC) dahili SPI
veri yoluna bağlı. Standart Linux sürücü yollarından hiçbiriyle erişilemiyor.
Kutudan çıktığı haliyle `fprintd-enroll` şunu söylüyor:

```
No devices available
```

Bu paket sorunu uçtan uca çözüyor:

- **Çekirdek modülü** — eSPI posta kutusu taşıması + GPIO el sıkışması; `/dev/gxfp` oluşturur
- **Kullanıcı alanı araçları** — TLS-PSK yönetimi, ham görüntü yakalama, tanılar
- **libfprint çatallı sürümü** — GXFP sürücüsü + SIGFM eşleştirme algoritması
- **PAM entegrasyonu** — `sudo` ve giriş ekranı parmak izini kabul eder

## Desteklenen Donanım

| Dizüstü | ACPI ID | Donanım Yazılımı | Durum |
|---|---|---|---|
| Huawei MateBook D16 2024 (MCLF-XX / M1010) | `GXFP5130:00` | `GF_GCC_EC_20067` | ✅ Doğrulandı |
| GXFP5130:00 bulunan diğer MateBook modelleri | `GXFP5130:00` | bilinmiyor | ❓ Test edilmedi — lütfen bildirin |

Sensörün mevcut olup olmadığını kontrol etmek için:

```sh
find /sys/bus/acpi/devices -name 'GXFP5130*'
```

## Gereksinimler

**Arch Linux:**

```sh
sudo pacman -S --needed base-devel linux-headers dkms cmake meson ninja \
  mbedtls glib2 libgusb gusb pixman nss libgudev cairo opencv doctest fprintd
```

LTS çekirdeği kullanıyorsanız `linux-lts-headers` paketini tercih edin. Diğer
dağıtımlar için şunlara ihtiyaç var: C/C++ derleyici, CMake ≥ 3.16, Meson,
Ninja, Mbed TLS, GLib 2, GUsb, pixman, NSS, Cairo, OpenCV ≥ 4.5, fprintd.

## Hızlı Başlangıç

```sh
# 1. Derle (root gerekmez)
./scripts/doctor.sh
./scripts/build.sh

# 2. Taşıma katmanını ve DKMS modülünü kur
sudo ./scripts/install.sh
sudo modprobe gxfp
./scripts/verify.sh

# 3. TLS PSK'yı yükle ve yakalamayı doğrula
sudo ./scripts/provision-psk.sh
sudo gxfp_capture --psk-raw32 /var/lib/fprintd/gxfp/psk_raw32.bin
```

Windows ile çift önyükleme yapıyorsanız ve sensör daha önce Windows üzerinde
yapılandırıldıysa mevcut anahtarı PSK'nın yerine kullanın:
[userspace/PSK.md](userspace/PSK.md)

## libfprint Entegrasyonu

Bu çatal, dağıtımın standart `libfprint` paketinin yerine geçer. Arch'ta kurulum
ve kaldırma işlemlerinin takip edilebilmesi için pacman paketi oluşturun:

```sh
./scripts/build-arch-package.sh
sudo pacman -U config/arch/libfprint-gxfp-*.pkg.tar.zst
sudo systemctl restart fprintd
fprintd-enroll
fprintd-verify
```

Dağıtım paketine geri dönmek için: `sudo pacman -S libfprint`

Diğer dağıtımlar için yerel kurulum:

```sh
sudo meson install -C build/libfprint
sudo ldconfig
sudo systemctl restart fprintd
fprintd-enroll
fprintd-verify
```

Hata ayıklama için ayrıntılı günlük:

```sh
sudo systemctl stop fprintd
sudo env FP_GXFP_LOG=1 /usr/lib/fprintd
```

## PAM Entegrasyonu (sudo + Giriş Ekranı)

`/etc/pam.d/system-auth` dosyasında şifre satırının üstüne `pam_fprintd.so`
ekleyin:

```
auth    required    pam_faillock.so   preauth
auth    sufficient  pam_fprintd.so timeout=10   ← bu satırı ekleyin
auth    [success=2 default=ignore]  pam_systemd_home.so
auth    [success=1 default=bad]     pam_unix.so  try_first_pass nullok
```

`timeout=10` önemli — olmadan şifre girildiğinde parmak izi modülü ~120 sn
timeout bekleyip sonra şifreye düştüğü için giriş çok yavaş olur.

Aynı değişikliği `/etc/pam.d/system-login` dosyasına eklemek ekran yöneticisini
de kapsar.

## Mimari

```
Tarayıcı / sudo / giriş ekranı
        │
        ▼
   pam_fprintd.so
        │
        ▼
    fprintd  ←─── libfprint (GXFP sürücüsü + SIGFM eşleştirici)
        │
        ▼
   /dev/gxfp  (karakter aygıtı, DKMS çekirdek modülü)
        │
        ▼
  eSPI posta kutusu @ 0xFE800000
  GPIO 816 darbesi → EC → GXFP5130 çipi (EC dahili SPI)
  GPIO 813 darbesi ← EC  (yanıt hazır)
```

İşlemci çiple doğrudan konuşamaz. Her komut, eSPI posta kutusu aracılığıyla
Gömülü Denetleyici üzerinden geçer. Çekirdek modülü taşımayı yönetir;
kullanıcı alanı basit bir okuma/yazma karakter aygıtı görür.

## Sorun Giderme

| Belirti | Olası neden | Çözüm |
|---|---|---|
| `/sys/bus/acpi/devices` altında `GXFP5130:00` yok | BIOS'ta parmak izi okuyucu kapalı | BIOS/UEFI'den etkinleştirin |
| `modprobe gxfp` sonrası `/dev/gxfp` yok | Çekirdek/modül sürüm uyumsuzluğu | `modinfo gxfp` vermagic ile `uname -r` karşılaştırın |
| fprintd'den `Operation not permitted` | `DeviceAllow` drop-in eksik | `sudo ./scripts/install.sh` çalıştırın, fprintd'yi yeniden başlatın |
| TLS MAC doğrulama hatası | Ana PSK sensör PSK'sıyla eşleşmiyor | `provision-psk.sh` ile yeniden yükleyin veya Windows'tan çıkarın |
| Yakalama çalışıyor, kayıt başarısız | libfprint/SIGFM katman sorunu | `FP_GXFP_LOG=1` çıktısı toplayın ve bir sorun bildirin |
| Parmak kaldırma zaman zaman algılanmıyor | `GF_GCC_EC_20067` donanım yazılımı özgünlüğü | Sürücü otomatik yeniden deniyor; işlem gerekmez |
| Çekirdek yükseltmesinden sonra modül yok | DKMS yeniden derleme gerekiyor | `sudo dkms autoinstall -k "$(uname -r)"` |

## Kaldırma

```sh
sudo ./scripts/uninstall.sh
```

Kaldırma betiği PSK'yı veya kayıtlı parmak izlerini silmez.

## Katkıda Bulunma

[CONTRIBUTING.md](.github/CONTRIBUTING.md) dosyasına bakın.

README'de listelenmeyen bir MateBook modelinde sürücü çalışıyorsa, lütfen bir
[uyumluluk raporu](https://github.com/Metrohan/gxfp5130-linux/issues/new?template=compatibility_report.yml)
açın — iki dakika sürer ve aynı donanıma sahip herkese yardımcı olur.

## Lisans

Çekirdek modülü: [GPL-2.0-only](LICENSE)
libfprint çatalı: [LGPL-2.1+](libfprint/COPYING)
Kullanıcı alanı araçları: GPL-2.0-only

## Kaynak Kodun Kökeni

- Çekirdek taşıması: [`Void755/gxfp_linux_driver`](https://github.com/Void755/gxfp_linux_driver), anlık görüntü `594c372`
- Kullanıcı alanı kütüphanesi/araçları: [`Void755/gxfpmoc`](https://github.com/Void755/gxfpmoc), anlık görüntü `4b489a7`
- libfprint çatalı: [`Void755/libfprint`](https://github.com/Void755/libfprint), anlık görüntü `1f7941e` + OpenCV 5 desteği

Bu yığın deneyseldir ve Goodix veya Huawei ile herhangi bir bağlantısı yoktur.
