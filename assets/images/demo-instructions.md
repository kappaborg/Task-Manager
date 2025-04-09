# TaskManager Demo Kayıt Talimatları

Bu belgede, TaskManager uygulamasının etkileşimli arayüzünü kullanarak nasıl demo kaydı yapılacağına dair adım adım talimatlar bulunmaktadır. Bu demo, kullanıcı kılavuzundaki "Üç Adet Görev Planlama ve Önceliklerini Değiştirme" örneğini takip etmektedir.

## Demo Kaydı İçin Hazırlık

1. Ekran kaydı uygulamanızı hazırlayın (Mac için Kap veya GIPHY Capture önerilir)
2. Terminal uygulamanızı açın ve TaskManager klasörüne gidin
3. Demo sırasında `/TaskManager/` dizininde olduğunuzdan emin olun 
4. Temiz başlangıç için `task_log.txt`, `system_files.txt` ve `browser_status.txt` dosyalarını temizleyin:
   ```
   echo "" > task_log.txt
   echo "" > system_files.txt
   echo "" > browser_status.txt
   ```


### 2. Üç Adet Task Ekleme
- **İlk Task**: 
  - Ana menüden `11` girin (Task Scheduler)
  - `2` girin (Add new task)
  - Komut olarak `echo "Bu ilk görevimiz" >> task_log.txt` girin
  - Zamanlama tipi olarak `2` (Run at intervals) girin
  - Zaman aralığı olarak `60` (saniye) girin
  
- **İkinci Task**: 
  - Yine `2` girin (Add new task)
  - Komut olarak `ls -la >> system_files.txt` girin
  - Zamanlama tipi olarak `3` (Run daily at specific time) girin
  - Çalışma zamanı olarak şu formatta bir zaman girin: `YYYY MM DD HH MM` 
    (Örn: mevcut tarihi ve 2 dakika sonrasını kullanın)
  
- **Üçüncü Task**: 
  - Yine `2` girin (Add new task)
  - Komut olarak `ps aux | grep firefox > browser_status.txt` girin
  - Zamanlama tipi olarak `1` (Run once at specific time) girin
  - Çalışma zamanı olarak şu formatta bir zaman girin: `YYYY MM DD HH MM`
    (Örn: mevcut tarihi ve 3 dakika sonrasını kullanın)

### 3. Task Listesini Görüntüleme ve Zamanlayıcıyı Başlatma
- `1` girin (List scheduled tasks) ve üç task'ı görüntüleyin
- `4` girin (Start scheduler) ve zamanlayıcıyı başlatın
- `0` girin ve ana menüye dönün

### 4. Task PID'lerini Bulma ve Öncelik Değiştirme
- Ana menüden `1` girin (List all processes)
- `2` girin (Filter processes by name)
- Filtre olarak `task` yazın
- Ekranda görünen PID'leri not alın
- Ana menüye dönmek için `0` girin
  
- Her task için öncelik değiştirme:
  - Ana menüden `5` girin (Change process priority)
  - İlk task'ın PID'sini girin
  - Yüksek öncelik için `-10` girin
  
  - Tekrar `5` girin
  - İkinci task'ın PID'sini girin
  - Normal öncelik için `0` girin
  
  - Tekrar `5` girin
  - Üçüncü task'ın PID'sini girin
  - Düşük öncelik için `10` girin

### 5. Son Durumu Görüntüleme
- Ana menüden `1` girin (List all processes)
- Tüm süreçleri görüntüleyin
- `2` girin (Filter processes by name)
- Filtre olarak `task` yazın ve değişen öncelikleri gözlemleyin

## Kayıt Sonrası

1. Kaydı durdurun ve `.gif` formatında kaydedin
2. Kaydettiğiniz dosyayı `assets/taskmanager-demo.gif` olarak adlandırın
3. Şu komutları kullanarak GitHub'a yükleyin:
   ```
   git add assets/taskmanager-demo.gif
   git commit -m "Add TaskManager demo GIF"
   git push origin master
   ```

Kayıt tamamlandıktan sonra, README.md dosyasındaki "Screenshots & Demos" bölümünü güncelleyip GIF'i görüntüleyebilirsiniz. 