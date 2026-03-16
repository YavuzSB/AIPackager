AIPackager/
├── CMakeLists.txt              # Ana orkestratör (Tüm projeyi ve hedefleri yönetir)
├── core/                       # İş Mantığı (CLI ve GUI buraya bağımlıdır)
│   ├── CMakeLists.txt          # Core için statik kütüphane (static library) üretir
│   ├── include/core/
│   │   ├── Packager.hpp        # 400KB'lık bölme ve birleştirme mantığı
│   │   └── Scanner.hpp         # Dosya tarama ve filtreleme (.gitignore mantığı)
│   └── src/
│       ├── Packager.cpp
│       └── Scanner.cpp
├── cli/                        # Terminal Sürümü
│   ├── CMakeLists.txt          # 'aipackager-cli' çalıştırılabilir dosyasını üretir
│   └── src/main.cpp            # Sadece argümanları ayrıştırır ve Core'u çağırır
└── gui/                        # Masaüstü Sürümü
    ├── CMakeLists.txt          # 'aipackager-gui' çalıştırılabilir dosyasını üretir (ImGui + GLFW)
    └── src/main.cpp            # Pencereyi çizer, sürükle-bırak eventlerini Core'a iletir



Sen kıdemli bir C++ (C++20) yazılım mimarısın. Amacımız, projeleri AI asistanları için 400KB'lık metin parçalarına bölen bir aracın sadece Core (Çekirdek) kütüphanesini yazmak. Arayüz (GUI) veya CLI kodu YAZMAYACAKSIN.

Kurallar ve Standartlar:

Sadece Modern C++20 kullanılacak. Dosya işlemleri için std::filesystem zorunludur.

RAII prensiplerine sıkı sıkıya uyulacak, raw pointer kullanılmayacak.

Çıktı olarak verilen kodlar core/include/core/ ve core/src/ dizin yapısına uygun olarak ayrılmalıdır.

Geliştirilecek Sınıflar:

Scanner: Verilen kök dizini tarayacak. .git, build, node_modules gibi klasörleri ve .exe, .dll, .png gibi binary dosyaları es geçecek gelişmiş bir filtreleme yapısına sahip olmalı.

Packager: Scanner'dan gelen dosya yollarını okuyacak. Her dosyanın başına ve sonuna ayırıcı başlıklar (örn: ==== FILE: src/main.cpp ====) ekleyecek.

ChunkManager: Packager'ın ürettiği veriyi bellek dostu bir şekilde tam olarak 400 KB sınırını aşmayacak parçalara (chunk) bölecek ve bellekte tutacak.

IndexBuilder: Taranan tüm dosyaların ve atlanan dosyaların ağaç yapısını barındıran bir INDEX.txt içeriği oluşturacak.