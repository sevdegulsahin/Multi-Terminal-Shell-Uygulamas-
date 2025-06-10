# 💻 Multi-Terminal Shell Uygulaması

## 📌 Proje Açıklaması

Bu proje, **birden fazla terminal penceresi arasında iletişim kurabilen**, GTK tabanlı bir terminal uygulamasıdır.  
Her terminal, bağımsız bir süreç (`process`) olarak çalışır ve **paylaşımlı bellek** ile iletişim kurar.

### 🚀 Temel Özellikler
- Komut çalıştırma ve geçmiş görüntüleme
- Genel ve özel mesajlaşma
- Dosya gönderme
- Tema değiştirme (aydınlık, karanlık, pembe)

---

## 🛠️ Gereksinimler

| Gereksinim        | Açıklama                                        |
|-------------------|-------------------------------------------------|
| `GTK 3.0`         | Arayüz tasarımı için gerekli                    |
| `GNU/Linux`       | POSIX uyumluluğu (paylaşımlı bellek, semaforlar)|
| `gcc`             | C kaynak dosyalarının derlenmesi için           |

---

## ⚙️ Kurulum

### 🔧 1. Bağımlılıkları Yükleyin

Ubuntu/Debian sistemleri için:
```bash
sudo apt-get update
sudo apt-get install libgtk-3-dev build-essential
```

### 🔨 2. Kaynak Kodları Derleyin

Proje dizininde şu komutu çalıştırın:
```bash
gcc -o multi_shell view.c controller.c model.c `pkg-config --cflags --libs gtk+-3.0`
```

### ▶️ 3. Uygulamayı Çalıştırın

Varsayılan olarak 2 terminal ile başlatmak için:
```bash
./multi_shell
```

Terminal sayısını belirtmek için (1–10 arası):
```bash
./multi_shell 3
```

---

## 🧑‍💻 Kullanım Kılavuzu

### 🔹 Komut Girişi
- Terminalin altındaki giriş alanına Linux komutları yazılabilir  
  Örnekler:
  ```bash
  ls
  pwd
  cd ..
  ls | grep txt
  ls > output.txt
  ```

### 🔹 Özel Komutlar

| Komut                      | Açıklama                                           |
|----------------------------|----------------------------------------------------|
| `@msg <mesaj>`             | Tüm terminallere mesaj gönderir                    |
| `@msgto <id> <mesaj>`      | Belirtilen terminale özel mesaj gönderir          |
| `@file <dosya_adı>`        | Dosya içeriğini tüm terminallere yollar           |

### 🔹 Mesajlaşma ve Geçmiş
- **Üst metin alanı:** Komut geçmişini gösterir.
- **Alt metin alanı:** Genel ve özel mesajlar (renk kodlu)
- **Yukarı / Aşağı tuşları:** Komut geçmişinde gezinme

### 🎨 Tema Değiştirme
- Sağ üstteki **tema düğmesi** ile:
  - Aydınlık
  - Karanlık
  - Pembe

---

## 📁 Dosya Yapısı

| Dosya/Fonksiyon     | Açıklama                                               |
|---------------------|--------------------------------------------------------|
| `shared.h`          | Paylaşımlı bellek yapıları ve sabitler                |
| `model.h/.c`        | Veri yönetimi ve paylaşımlı bellek işlemleri          |
| `controller.h/.c`   | Komut işleme ve terminal mantığı                       |
| `view.c`            | GTK tabanlı kullanıcı arayüzü                          |

> Her terminal `fork()` ile başlatılır ve diğerlerinden izole çalışır.  
> İletişim paylaşımlı bellek ve semaforlar aracılığıyla sağlanır.

---

## 📌 Örnek Kullanım Senaryosu

```bash
./multi_shell
```

1. **Terminal 1**'de:
   ```bash
   ls
   ```
   ➜ Çıktı komut geçmişinde görünür.

2. **Terminal 1**'den özel mesaj:
   ```bash
   @msgto 2 Merhaba!
   ```
   ➜ **Terminal 2**'de renkli şekilde görüntülenir.

3. Tema düğmesiyle **karanlık moda** geçiş yapılabilir.

---

## 📝 Notlar

- Maksimum **10 terminal** desteklenir.
- Uygulama kapatıldığında **paylaşımlı bellek ve semaforlar temizlenir**.
- Her terminal, ayrı bir süreç olduğu için çökme durumları diğerlerini etkilemez.
