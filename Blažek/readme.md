# RehabRotator Pro – Interaktivní Rehabilitační Pomůcka

## 📋 Přehled projektu
**RehabRotator Pro** je hardwarová a softwarová rehabilitační pomůcka určená pro centra zaměřená na pacienty s omezenou mobilitou a pohybovými problémy. Zařízení měří rozsah rotace (např. trupu nebo končetin) v reálném čase a převádí naměřené stupně na postupně se rozsvěcující LED pás.

Cílem je motivovat pacienta k dosažení maximálního pohybu (rozsvícení všech 64 LED diod) prostřednictvím okamžité vizuální zpětné vazby a interaktivních her.

---

## 🛠️ Popis Konstrukce a Hardware
* **LED Pás:** 1řádkový adresovatelný LED pás (WS2812B / NeoPixel) obsahující **64 LED diod**.
* **Středová Krabička:** Uprostřed tyče (na pozici **LED 28 až 36**) se nachází elektronická skříňka.
* **Elektronika uvnitř skříňky:**
  * **Řídicí jednotka:** Arduino Uno (případně ESP32)
  * **Senzor pohybu:** Gyro senzor / Akcelerometr **MPU-6050** (měření osy rotace Z)
  * **Wi-Fi modul:** ESP8266 nebo vestavěné Wi-Fi na ESP32 (přenos dat přes JSON / WebSockets)

---

## 💻 Webová Aplikace a Funkce

Aplikace (soubor `index.html`) funguje jako samostatná simulace i jako živý dashboard pro terapeuta a obsahuje 3 hlavní rozhraní:

1. **Režim pro Vývojáře (Debug & Simulace):**
   * Grafické znázornění 64 LED diod rozdělených na levý segment, středový box a pravý segment.
   * Možnost simulovat náklon pomocí šipek na klávesnici (`←` / `→`) nebo posuvníku.
   * Telemetrie v reálném čase: sledování úhlu (0–64°), procentuálního plnění a JSON streamu.

2. **Režim pro Pacienta (Gamifikace – Zalévání květiny):**
   * Interaktivní motivace pacientů.
   * **Princip hry:** Pacient musí vytočit rotaci na maximum (64°) a podržet pozici po dobu odpočtu (3 sekundy). Tím rostlinu zalije a ta vyroste / rozkvete (🌱 ➔ 🌿 ➔ 🪴 ➔ 🌻).
   * Automatické střídání cvičení na levou a pravou stranu pro vyvážený trénink.

3. **Záložka s Arduino Kódem:**
   * Obsahuje kompletní zdrojový kód (`RehabDevice.ino`) připravený k nahrání do Arduina včetně návodu na zapojení pinů.

---

## 🔌 Schéma Zapojení (Pinout)

| Komponenta | Pin na Arduinu | Poznamka |
| :--- | :--- | :--- |
| **MPU6050 (SDA)** | `A4` | I2C Komunikace |
| **MPU6050 (SCL)** | `A5` | I2C Komunikace |
| **MPU6050 (VCC/GND)** | `5V` / `GND` | Napájení senzoru |
| **WS2812B (DIN)** | `Digital Pin 6` | Datový pin (doporučen rezistor 470Ω) |
| **WS2812B (VCC/GND)** | `5V` / `GND` | Externí stabilizovaný zdroj 5V (min. 2–3A) |

---

## 🚀 Jak Začít

### 1. Spuštění Webové Simulace
1. Otevřete soubor `index.html` v jakémkoliv moderním webovém prohlížeči (Chrome, Firefox, Edge).
2. Přepínejte mezi záložkami **Vývojář**, **Hra (Zalévání)** a **Arduino Kód**.
3. Klávesami `←` a `→` nebo táhlem simulujte náklon tyče.

### 2. Nahrání Kódu do Arduina
1. Nainstalujte **Arduino IDE**.
2. Vložte do IDE knihovny:
   * `Adafruit_NeoPixel`
   * `MPU6050_tockn` (nebo standardní `Wire.h`)
3. Zkopírujte kód ze záložky **Arduino Kód** ve webové aplikaci a nahrajte jej do své desky.