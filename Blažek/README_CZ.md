# Rehab Motion Lab – Arduino Nano RP2040 Connect

Tato verze je připravená pro **Arduino Nano RP2040 Connect**. Deska má vestavěný Wi‑Fi modul NINA-W102 a vestavěný šestiosý IMU senzor LSM6DSOX. Výchozí firmware proto měří pohyb pomocí vestavěného senzoru; externí MPU6050 je podporovaný jako volitelná alternativa.

## 1. Jak funguje skutečné spojení

Arduino vytvoří vlastní Wi‑Fi přístupový bod a současně na portu 80 provozuje HTTP server. Webová aplikace je vložená přímo ve firmwaru, takže ji prohlížeč načítá přímo z Arduina. Není nutný ESP8266, počítačový server ani internetové připojení.

| Položka | Hodnota |
|---|---|
| Wi‑Fi SSID | `RehabMotion` |
| Wi‑Fi heslo | `Rehab2026` |
| Adresa aplikace | `http://192.168.4.1` |
| Datový endpoint | `http://192.168.4.1/api/data` |
| Nastavení cíle | `/api/config?target=25&hold=3000` |
| Kalibrace | `/api/calibrate` |
| Kontrola serveru | `/api/health` |
| Protokol | HTTP + JSON, dotaz každých 250 ms |

**Doporučený postup:** Po nahrání firmware se připoj k Wi‑Fi `RehabMotion` a v prohlížeči otevři `http://192.168.4.1`. Nepotřebuješ otevírat lokální HTML soubor – Arduino samo posílá stejnou stránku z flash paměti. Samostatný HTML soubor je přibalený pro kontrolu vzhledu a simulaci.

Pokud se přesto rozhodneš otevřít HTML soubor ručně z počítače, JavaScript zkusí připojení na `http://192.168.4.1`; prohlížeč může podle svých bezpečnostních nastavení vyžadovat povolení přístupu k místní síti. Proto je lepší otevřít stránku přímo na IP Arduina.

## 2. Co je v projektu

- `rehab_motion_lab.html` – samostatná webová stránka (HTML, CSS a JS v jednom souboru).
- `RehabMotionNanoRP2040.ino` – kompletní firmware; obsahuje stejnou HTML stránku a HTTP API.
- `README_CZ.md` – tento návod.

## 3. Potřebný hardware

- Arduino Nano RP2040 Connect.
- Adresovatelný LED pásek 64× WS2812B.
- Stabilizovaný externí zdroj 5 V, doporučeně 5 A.
- Převodník logických úrovní 3,3 V → 5 V, například 74AHCT125 nebo 74HCT245.
- Rezistor 330–470 Ω do datového vodiče.
- Kondenzátor alespoň 1000 µF / 6,3 V mezi 5 V a GND u vstupu LED pásku.
- USB kabel pro napájení/programování Nano.
- Volitelně modul MPU6050, pokud jej chceš použít místo senzoru vestavěného v desce.

### Důležité k napájení LED

**Nikdy nenapájej všech 64 LED z pinu 5V/3V3 nebo z USB desky Nano.** LED pásek napájej samostatným zdrojem 5 V. Zem externího zdroje (GND) musí být propojena se zemí Arduina (GND). Kladný vodič 5 V z LED zdroje připoj pouze k napájecímu vstupu LED pásku, ne na GPIO desky.

Nano RP2040 Connect pracuje s logikou 3,3 V. Proto veď data z GPIO18 přes převodník úrovní na datový vstup WS2812B. Rezistor zapoj sériově do datové linky a kondenzátor paralelně na napájení pásku se správnou polaritou.

64 LED může při bílé barvě a maximálním jasu vyžadovat až přibližně 3,8 A. Zdroj 5 V / 5 A poskytuje rezervu. Firmware používá tlumené barvy a nevyužívá maximální jas, ale zdroj a vodiče dimenzuj na možnou zátěž.

## 4. Zapojení

### Výchozí varianta – vestavěný senzor LSM6DSOX

Vestavěný akcelerometr a gyroskop už jsou na desce Nano RP2040 Connect. Nepřipojuje se žádný další senzor ani I²C vodiče. Desku mechanicky upevni do krabičky tak, aby se její osa X při požadovaném pohybu skutečně otáčela.

| Nano RP2040 Connect | Připojení |
|---|---|
| D6 (GPIO18) | vstup převodníku 3,3 V → 5 V pro DATA LED |
| GND | společná zem s LED zdrojem |
| USB | napájení a programování desky |

Pozor: knihovna `NeoPixelConnect` používá **číslo GPIO**, nikoliv označení pinu D6. V kódu je tedy `LED_GPIO = 18`, což odpovídá fyzickému pinu D6 podle pinoutu Nano RP2040 Connect.

### Volitelná varianta – externí MPU6050

V horní části `.ino` změň:

```cpp
#define USE_EXTERNAL_MPU6050 0
```

na:

```cpp
#define USE_EXTERNAL_MPU6050 1
```

Pak nainstaluj navíc knihovny `Adafruit MPU6050`, `Adafruit Unified Sensor` a `Adafruit BusIO`.

| MPU6050 | Nano RP2040 Connect |
|---|---|
| VCC | 3V3 (ověř, že tvůj modul podporuje napájení 3,3 V) |
| GND | GND |
| SDA | A4 / SDA |
| SCL | A5 / SCL |

Použij I²C adresu `0x68` (výchozí adresa v kódu). Pokud má tvůj modul adresu `0x69`, uprav `externalMPU.begin(0x68, &Wire)` na `externalMPU.begin(0x69, &Wire)`.

Vestavěný LSM6DSOX a externí MPU6050 jsou alternativní zdroje měření – firmware používá jen jeden z nich podle hodnoty přepínače.

## 5. Instalace Arduino IDE

1. Otevři Arduino IDE.
2. V Boards Manageru nainstaluj balíček **Arduino Mbed OS Nano Boards**.
3. Vyber desku **Arduino Nano RP2040 Connect**.
4. V Library Manageru nainstaluj:
   - `WiFiNINA`
   - `Arduino_LSM6DSOX`
   - `NeoPixelConnect`
5. Volitelně při použití externího MPU6050 také `Adafruit MPU6050`, `Adafruit Unified Sensor` a `Adafruit BusIO`.
6. Otevři `RehabMotionNanoRP2040.ino` a nahraj jej do desky.

Pro instalaci desky je potřeba právě balíček Arduino Mbed OS Nano Boards. NeoPixelConnect je zvolen proto, že je určený pro Nano RP2040 Connect a používá RP2040 PIO pro přesné časování WS2812. Obecná Adafruit NeoPixel knihovna měla na některých verzích tohoto Mbed jádra známé problémy s kompilací – proto není ve výchozím kódu použita.

## 6. První spuštění

1. Připoj LED pásek k externímu zdroji a datový vstup ke GPIO18 přes převodník úrovní.
2. Připoj Nano přes USB a otevři Serial Monitor na 115200 baud.
3. Po zapnutí nech lištu i krabičku několik sekund nehybně v neutrální poloze; firmware při startu automaticky kalibruje nulu.
4. V počítači nebo telefonu vyber Wi‑Fi `RehabMotion` a zadej heslo `Rehab2026`.
5. V prohlížeči otevři `http://192.168.4.1`.
6. V aplikaci zkontroluj stav připojení a úhel. Pro novou kalibraci použij tlačítko „Kalibrovat nulovou polohu“.

Wi‑Fi síť zařízení nemá přístup k internetu. Telefon nebo počítač může zobrazit hlášení „Bez internetu“; zvol možnost zůstat připojen k této síti.

## 7. Funkce webové aplikace

### Režim pro vývojáře

- zobrazuje 64 LED a středovou krabičku na pozicích 28–36;
- ukazuje živý úhel, procentuální progres a počet rozsvícených LED;
- vypisuje surové hodnoty akcelerometru (g) a gyroskopu (°/s);
- obsahuje simulaci přes posuvník a šipky klávesnice;
- umožňuje nastavit cílový úhel a dobu výdrže;
- poskytuje kalibraci nulové polohy.

### Režim pro pacienta

- úkol „Zalévání květiny“;
- pacient dosáhne cílového úhlu a udrží jej po nastavenou dobu;
- po dokončení výdrže květina rozkvete;
- aplikace automaticky přepne požadovaný směr a procvičí obě strany.

## 8. Nastavení osy a směru

Výchozí výpočet měří náklon kolem osy X senzoru:

```cpp
atan2f(ay, az)
```

a používá složku gyroskopu `gx`. Pokud je senzor v krabičce otočený jinak, úhel může reagovat na jinou osu nebo může být směr obrácený. V takovém případě je potřeba změnit výpočet osy. Pokud je správná osa, ale znaménko je opačné, změň:

```cpp
const float SENSOR_SIGN = 1.0f;
```

na `-1.0f`.

Při prvním testu zkontroluj v režimu vývojáře, že pomalý pohyb na levou stranu ukazuje záporný úhel a pohyb doprava kladný úhel. Kalibraci prováděj bez pohybu.

## 9. API

| Endpoint | Výsledek |
|---|---|
| `GET /` | Vrátí webovou aplikaci |
| `GET /api/data` | Úhel, progres, stav senzoru, LED count, surová data |
| `GET /api/config?target=25&hold=3000` | Nastaví cíl 25° a výdrž 3000 ms |
| `GET /api/calibrate` | Znovu nastaví nulovou polohu |
| `GET /api/health` | Základní kontrola HTTP serveru |

Aplikace provádí GET požadavek na `/api/data` každých 250 ms. Nepoužívá jen simulovaná data, pokud je simulace v ovládacím panelu vypnutá.

## 10. Řešení problémů

**Wi‑Fi síť RehabMotion se neobjeví**
- Zkontroluj, že máš vybranou správnou desku `Arduino Nano RP2040 Connect`.
- Otevři Serial Monitor na 115200 baud a zkontroluj hlášení o modulu NINA.
- V Arduino IDE spusť příklad kontroly firmware WiFiNINA / FirmwareUpdater a ověř, že firmware modulu NINA odpovídá nainstalované knihovně WiFiNINA.
- Zkus zařízení vypnout a znovu zapnout.

**Stránka se načte, ale ukazuje chybu senzoru**
- U výchozí varianty zkontroluj inicializaci vestavěného LSM6DSOX v Serial Monitoru.
- Při startovní kalibraci nech zařízení bez pohybu.
- Pokud používáš externí MPU6050, nastav `USE_EXTERNAL_MPU6050 1`, zkontroluj adresu I²C a zapojení A4/A5.

**Úhel se při pohybu téměř nemění**
- Zkontroluj orientaci desky uvnitř krabičky. Výchozí firmware měří osu X.
- Pokud reaguje jiná osa, uprav výpočet akceleračního úhlu i odpovídající složku gyroskopu.
- Pro obrácený směr změň `SENSOR_SIGN` na `-1.0f`.

**LED pásek nesvítí nebo bliká**
- Ověř externí 5V zdroj, společnou zem, správný směr DATA a převodník 3,3 V → 5 V.
- Ověř, že DATA vstup je připojen k fyzickému D6 (GPIO18), ne k jiné LED pozici.
- Zkontroluj pořadí RGB a typ pásku (WS2812B).
- Nepřipojuj napájení pásku k 3V3 pinu Nano.

## 11. Omezení a bezpečnost

- Kód je připravený pro konkrétní desku, ale nemohl být fyzicky otestován na tvém kusu hardwaru. Ověř nejprve Wi‑Fi, senzor a LED zvlášť.
- Výsledek filtrace závisí na orientaci desky a mechanickém uchycení. Před skutečným cvičením proveď kalibraci a zkontroluj znaménko i rozsah.
- Nepoužívej aplikaci jako zdravotnický prostředek ani jako jediný bezpečnostní dohled při rehabilitaci. Cílový úhel musí určit odborník podle konkrétního pacienta. Při bolesti nebo nepohodlí cvičení zastav.
- LED pás musí mít vlastní správně dimenzované napájení a jištění. Zajisti, aby se pacient nemohl dotknout odkrytých vodičů ani horkých částí.
