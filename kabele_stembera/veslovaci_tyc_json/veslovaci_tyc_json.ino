/*
  Rehabilitační veslařská tyč - verze s JSON výstupem (upraveno pro Arduino Nano RP2040 Connect)

  - Vestavěný LSM6DSOX měří náklon, detekce záběrů, tempo, LED pásek
  - Data se připravují ve formátu JSON a posílají:
      1) přes USB (Serial, 1 řádek JSON každých 100 ms)
      2) přes WiFi: http://<IP>/data (JSON), http://<IP>/reset (vynuluje počítadlo)

  Knihovny: NeoPixelConnect, Arduino_LSM6DSOX, WiFiNINA
*/

#include <NeoPixelConnect.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <SPI.h>
#include <math.h>

// ====================== NASTAVENÍ ======================
#define LED_PIN        4       // Pin D4 pro vaši rehabilitační tyč
#define NUM_LEDS       64      // Počet LED na vaší tyči
#define LED_BRIGHTNESS 80      // Jas 0-255

#define USE_WIFI       1       // 1 = zapnout WiFi a webový server, 0 = jen USB
const char WIFI_SSID[] = "NAZEV_WIFI";
const char WIFI_PASS[] = "HESLO_WIFI";

const float ANGLE_HIGH = 15.0;
const float ANGLE_LOW  = -15.0;
const unsigned long MIN_STROKE_MS = 1000;
const unsigned long TIMEOUT_MS    = 5000;

const float TARGET_SPM    = 22.0;
const float TOLERANCE_SPM = 10.0;

#define TILT_AXIS 0            // 0 = osa X (podélná osa tyče), 1 = osa Y
// =======================================================

NeoPixelConnect strip(LED_PIN, NUM_LEDS, pio0, 0);

float angle = 0;
bool  wasLow = false;
unsigned long lastStrokeMs = 0;
unsigned long strokeCount  = 0;
float spm = 0;
float score = 0;
float displayLevel = 0;
unsigned long flashUntil = 0;
unsigned long lastJson = 0;

#if USE_WIFI
WiFiServer server(80);
#endif

// ---------------------- PŘÍPRAVA DAT VE FORMÁTU JSON ----------------------
String buildJson() {
  String j;
  j.reserve(160);
  j  = "{\"t\":";         j += millis();
  j += ",\"angle\":";     j += String(angle, 1);
  j += ",\"strokes\":";   j += strokeCount;
  j += ",\"spm\":";       j += String(spm, 1);
  j += ",\"target\":";    j += String(TARGET_SPM, 0);
  j += ",\"score\":";     j += String(score, 2);
  j += ",\"leds\":";      j += (int)(displayLevel + 0.5);
  j += ",\"numLeds\":";   j += NUM_LEDS;
  j += "}";
  return j;
}

// ---------------------- ČTENÍ AKCELEROMETRU (LSM6DSOX) ----------------------
bool readAngle(float &outAngle) {
  if (!IMU.accelerationAvailable()) return false;

  float fx, fy, fz;
  if (!IMU.readAcceleration(fx, fy, fz)) return false;

  if (TILT_AXIS == 0) outAngle = atan2(fx, sqrt(fy * fy + fz * fz)) * 180.0 / PI;
  else                outAngle = atan2(fy, sqrt(fx * fx + fz * fz)) * 180.0 / PI;
  return true;
}

// ---------------------- LED (NeoPixelConnect) ----------------------
void setLedScaled(int index, uint8_t r, uint8_t g, uint8_t b) {
  uint8_t sr = (r * LED_BRIGHTNESS) / 255;
  uint8_t sg = (g * LED_BRIGHTNESS) / 255;
  uint8_t sb = (b * LED_BRIGHTNESS) / 255;
  strip.neoPixelSetValue((uint8_t)index, sr, sg, sb, false);
}

void showLeds() {
  float target = score * NUM_LEDS;
  displayLevel += (target - displayLevel) * 0.1;

  strip.neoPixelClear(false);

  // Krátké modré probliknutí při dokončení záběru
  if (millis() < flashUntil) {
    for (int i = 0; i < NUM_LEDS; i++) {
      setLedScaled(i, 0, 0, 60);
    }
  }

  // Vykreslení sloupce podle skóre (plynulý přechod z červené do zelené)
  int lit = (int)(displayLevel + 0.5);
  for (int i = 0; i < lit && i < NUM_LEDS; i++) {
    float s = (float)i / (NUM_LEDS - 1);
    uint8_t r = (uint8_t)(255 * (1.0 - s));
    uint8_t g = (uint8_t)(255 * s);
    setLedScaled(i, r, g, 0);
  }

  strip.neoPixelShow();
}

// ---------------------- Detekce záběru ----------------------
void onStroke(unsigned long now) {
  strokeCount++;
  flashUntil = now + 120;
  if (lastStrokeMs != 0) {
    float instSpm = 60.0 / ((now - lastStrokeMs) / 1000.0);
    spm = (spm == 0) ? instSpm : (0.6 * spm + 0.4 * instSpm);
  }
  lastStrokeMs = now;
}

void detectStroke(unsigned long now) {
  if (angle < ANGLE_LOW) wasLow = true;
  if (wasLow && angle > ANGLE_HIGH) {
    if (lastStrokeMs == 0 || now - lastStrokeMs >= MIN_STROKE_MS) onStroke(now);
    wasLow = false;
  }
  if (lastStrokeMs != 0 && now - lastStrokeMs > TIMEOUT_MS) {
    spm = 0;
    lastStrokeMs = 0;
  }
}

// ---------------------- WiFi server (WiFiNINA) ----------------------
#if USE_WIFI
void wifiSetup() {
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("{\"info\":\"WiFi modul nenalezen\"}");
    return;
  }

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.println("{\"info\":\"pripojuji WiFi\"}");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("{\"info\":\"WiFi OK\",\"ip\":\"");
    Serial.print(WiFi.localIP());
    Serial.println("\"}");
  } else {
    Serial.println("{\"info\":\"WiFi se nepripojila\"}");
  }
  server.begin();
}

void handleWebClients() {
  WiFiClient client = server.available();
  if (!client) return;

  String reqLine = "";
  String currentLine = "";
  unsigned long timeout = millis();

  while (client.connected() && millis() - timeout < 200) {
    if (client.available()) {
      char c = client.read();
      if (c == '\n') {
        if (currentLine.length() == 0) {
          if (reqLine.indexOf("GET /reset") >= 0) {
            strokeCount = 0; spm = 0; lastStrokeMs = 0; wasLow = false;
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: application/json");
            client.println("Access-Control-Allow-Origin: *");
            client.println("Connection: close\r\n");
            client.println("{\"ok\":true}");
          } else {
            // Výchozí odpověď (/data i /) vrací JSON
            client.println("HTTP/1.1 200 OK");
            client.println("Content-Type: application/json");
            client.println("Access-Control-Allow-Origin: *");
            client.println("Connection: close\r\n");
            client.println(buildJson());
          }
          break;
        }
        if (reqLine.length() == 0) reqLine = currentLine;
        currentLine = "";
      } else if (c != '\r') {
        currentLine += c;
      }
    }
  }
  client.stop();
}
#endif

// ---------------------- Arduino ----------------------
void setup() {
  Serial.begin(115200);
  delay(1500);

  if (!IMU.begin()) {
    Serial.println("{\"error\":\"IMU nenalezeno\"}");
  }

  strip.neoPixelClear(true);

#if USE_WIFI
  wifiSetup();
#endif
}

void loop() {
#if USE_WIFI
  handleWebClients();
#endif

  static unsigned long lastSample = 0;
  unsigned long now = millis();
  if (now - lastSample < 10) return;
  lastSample = now;

  float raw;
  if (readAngle(raw)) {
    angle = 0.8 * angle + 0.2 * raw;
    detectStroke(now);
  }

  score = 0;
  if (spm > 0) {
    score = 1.0 - fabs(spm - TARGET_SPM) / TOLERANCE_SPM;
    score = constrain(score, 0.0, 1.0);
  }
  showLeds();

  // JSON řádek přes USB (pro webovou stránku v režimu USB)
  if (now - lastJson >= 100) {
    lastJson = now;
    Serial.println(buildJson());
  }
}