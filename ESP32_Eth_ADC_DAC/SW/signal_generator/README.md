# Signal Generator + Monitor

ESP32-ETH01 + WM8782S ADC + PCM5102A DAC sa web kontrolama preko Ethernet/WiFi.

---

## Šta projekat radi

1. ESP32 generiše stereo audio signal (sinus / kvadrat / trougao) i šalje ga DAC-u preko I²S
2. DAC ga pretvara u analogni signal koji ide u zvučnik / loopback ka ADC-u
3. ADC snima taj signal i šalje ga nazad ESP32-u
4. ESP32 analizira primljeni signal (frekvencija, RMS amplituda)
5. Web stranica prikazuje rezultate u realnom vremenu i omogućava kontrolu

---

## Sadržaj foldera

```
signal_generator/
├── signal_generator.ino     ← glavni Arduino fajl
├── webpage.h                ← HTML/CSS/JS web stranica
└── README.md                ← ovaj fajl
```

**Važno:** folder MORA da se zove `signal_generator` jer je glavni fajl `signal_generator.ino`. Arduino IDE zahteva da se folder zove isto kao i glavni .ino fajl.

---

## Priprema Arduino IDE-a

### 1. Instalacija ESP32 board package-a

Verovatno već imaš (verzija 3.3.8). Ako ne:
- File → Preferences → Additional Boards Manager URLs:
  `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
- Tools → Board → Boards Manager → traži "esp32" → instaliraj

### 2. Instalacija dodatnih biblioteka

Sketch → Include Library → Manage Libraries → traži i instaliraj:

| Biblioteka | Autor |
|---|---|
| **WebSockets** | Markus Sattler |

Ostale (`WebServer`, `WiFi`, `ETH`, `driver/i2s.h`) dolaze sa ESP32 board package-om.

### 3. Selekcija board-a

- Tools → Board → esp32 → **"WT32-ETH01"**

Ako "WT32-ETH01" nije izlistan, izaberi **"ESP32 Dev Module"** — sve radi isto.

### 4. Upload settings

- Upload Speed: **115200**
- CPU Frequency: **240MHz**
- Flash Frequency: **40MHz**
- Flash Size: **4MB (32Mb)**
- Partition Scheme: **Default 4MB**

---

## Upload koda na ESP32

ESP32-ETH01 nema USB port — koristi USB-UART dongle, baš kao za blink test.

### Povezivanje za upload

```
USB-UART dongle    ESP32-ETH01
─────────────────  ─────────────
    TX        →    pin 7  (RXD)
    RX        →    pin 8  (TXD)
    GND       →    pin 2  (GND)
    5V        →    pin 12 (5V)
```

### Procedura uploada

1. **Pripremi za boot mod:** spoji IO0 (pin 24) na GND
2. **Resetuj ESP32:** kratko prekini napajanje (skini i vrati USB dongle)
3. U Arduino IDE klikni **Upload**
4. Sačekaj da se završi (~30 sekundi)
5. **Otkači IO0 sa GND-a**
6. Ponovo resetuj ESP32 (prekini napajanje)

Ako ne ide, proveri da li su TX/RX žice zamenjene — to je najčešća greška.

---

## Pristup web stranici

### Preko Ethernet kabla (default)

1. Spoji RJ45 kabl od ESP32-ETH01 na router (ili direktno na laptop)
2. Otvori **Serial Monitor** u Arduino IDE (115200 baud)
3. Resetuj ESP32 — videćeš ispis sa IP adresom, npr:
   ```
   [ETH] IP adresa: 192.168.1.123
   Otvori u pretrazivacu: http://192.168.1.123
   ```
4. Otvori tu IP adresu u pretraživaču na laptopu/telefonu

### Preko WiFi-ja (fallback)

Ako Ethernet kabl nije povezan u roku od 3 sekunde od starta, ESP32 automatski pokreće WiFi Access Point:

1. Na telefonu/laptopu uđi u WiFi podešavanja
2. Poveži se na mrežu **"ESP32-Signal-Generator"** sa lozinkom **"12345678"**
3. Otvori u pretraživaču: **http://192.168.4.1**

---

## Korišćenje web stranice

Stranica ima dva dela:

### Levi kanal (L) i Desni kanal (R)

Za svaki kanal možeš nezavisno podesiti:
- **Oblik talasa**: Sinus / Kvadrat / Trougao
- **Frekvencija**: klizač 100–5000 Hz
- **Amplituda**: klizač 0–100%
- **Mute**: dugme za isključivanje kanala

### Analiza primljenog signala (loopback)

Ispod kontrola, za svaki kanal prikazuje se:
- **Detektovana frekvencija** — koja bi trebalo da bude jednaka onoj koju šalješ (ovo dokazuje da loopback radi)
- **RMS amplituda** — efektivna jačina signala

### Oscilogram

Donji deo stranice prikazuje grafiku primljenog signala (oba kanala) u realnom vremenu, 10 puta u sekundi.

---

## Šta očekivati pri prvom pokretanju

Posle uspešnog uploada i resetovanja, na **Serial Monitor**-u trebalo bi da vidiš:

```
================================================
  Signal Generator + Monitor
  ESP32-ETH01 + WM8782S ADC + PCM5102A DAC
================================================

[INIT] Pokusavam Ethernet...
[ETH] Start
[ETH] Kabl povezan
[ETH] IP adresa: 192.168.1.123
[INIT] Ethernet aktivan
[I2S] Inicijalizacija TX (DAC) - master mod...
[I2S TX] OK
[I2S] Inicijalizacija RX (ADC) - slave mod...
[I2S RX] OK
[HTTP] Server pokrenut na portu 80
[WS] Server pokrenut na portu 81

================================================
  Otvori u pretrazivacu: http://192.168.1.123
================================================
```

---

## Troubleshooting

### Problem: Upload ne uspeva

- IO0 mora biti na GND tokom uploada
- TX/RX žice mogu biti zamenjene
- Resetuj ESP32 pre kliktanja Upload

### Problem: Stranica se učitava ali analiza pokazuje 0 Hz

- Loopback (DAC LROUT → ADC Lin) nije fizički povezan
- Proveri jumpere JP5 i JP6 — moraju biti zatvoreni
- DAC XSMT pin mora biti na 3.3V (un-mute)

### Problem: Detektovana frekvencija je dvostruko veća/manja

- Sample rate ADC i DAC nisu sinhronizovani
- Proveri DIP switch na ADC modulu — svi OFF (default)

### Problem: Šum / pucketanje u signalu

- Loš jumper kabl ili labava veza
- Proveri da li su sve GND linije spojene
- Možda treba decoupling kondenzator (100nF + 10μF) između VIN i GND na svakom modulu

### Problem: Nema pristupa web stranici

- Proveri IP adresu na Serial Monitor-u
- Proveri da li si u istoj mreži kao ESP32
- Probaj ping na IP adresu sa laptopa

---

## Tehnički detalji

| Parametar | Vrednost |
|---|---|
| Sample rate | 48 kHz |
| Bit dubina | 16-bit signed |
| Kanali | Stereo (L + R nezavisno) |
| I²S TX kontroler | I2S_NUM_0, ESP32 master |
| I²S RX kontroler | I2S_NUM_1, ESP32 slave (ADC je master) |
| Mreža | Ethernet + WiFi AP fallback |
| HTTP port | 80 |
| WebSocket port | 81 |
| Real-time prikaz | 10× u sekundi |

---

## Pinovi (rekapitulacija prema KiCad šemi)

| Funkcija | ESP32 GPIO | Komponenta |
|---|---|---|
| I²S TX BCLK | IO15 | DAC pin 2 (BCK) |
| I²S TX LRCK | IO14 | DAC pin 4 (LCK) |
| I²S TX DATA | IO4 | DAC pin 3 (DIN) |
| I²S RX BCLK | IO36 | ADC pin 1 (B) |
| I²S RX LRCK | IO39 | ADC pin 2 (LR) |
| I²S RX DATA | IO35 | ADC pin 3 (D) |

---

Marija Sekulić · RTRK · jun 2026
