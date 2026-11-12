/*
  Rehabilitační veslařská tyč - verze s JSON výstupem

  - MPU6050 (I2C) měří náklon, detekce záběrů, tempo, LED pásek (jako dříve)
  - NOVĚ: data se připravují ve formátu JSON a posílají
      1) přes USB (Serial, 1 řádek JSON každých 100 ms)  -> funguje na Uno/Nano/ESP32
      2) přes WiFi na ESP32: http://<IP>/data  (JSON), http://<IP>/reset (vynuluje počítadlo)

  Ukázka JSON:
  {"t":12345,"angle":12.3,"strokes":14,"spm":21.4,"target":22,"score":0.87,"leds":26,"numLeds":30}

  Knihovny: Adafruit NeoPixel
*/

#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#if defined(ESP32)
  #define USE_WIFI 1
  #include <WiFi.h>
  #include <WebServer.h>
#else
  #define USE_WIFI 0
#endif

// ====================== NASTAVENÍ ======================
#define LED_PIN        6       // u ESP32 změň třeba na 5
#define NUM_LEDS       30
#define LED_BRIGHTNESS 80

const char* WIFI_SSID = "NAZEV_WIFI";
const char* WIFI_PASS = "HESLO_WIFI";

const float ANGLE_HIGH = 15.0;
const float ANGLE_LOW  = -15.0;
const unsigned long MIN_STROKE_MS = 1000;
const unsigned long TIMEOUT_MS    = 5000;

const float TARGET_SPM    = 22.0;
const float TOLERANCE_SPM = 10.0;

#define TILT_AXIS 0            // 0 = osa X, 1 = osa Y
// =======================================================

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
const int MPU_ADDR = 0x68;

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
WebServer server(80);
#endif

// ---------------------- PŘÍPRAVA DAT VE FORMÁTU JSON ----------------------
String buildJson() {
  String j;
  j.reserve(160);
  j  = "{\"t\":";        j += millis();
  j += ",\"angle\":";    j += String(angle, 1);
  j += ",\"strokes\":";  j += strokeCount;
  j += ",\"spm\":";      j += String(spm, 1);
  j += ",\"target\":";   j += String(TARGET_SPM, 0);
  j += ",\"score\":";    j += String(score, 2);
  j += ",\"leds\":";     j += (int)(displayLevel + 0.5);
  j += ",\"numLeds\":";  j += NUM_LEDS;
  j += "}";
  return j;
}

// ---------------------- MPU6050 ----------------------
void mpuInit() {
  Wire.begin();
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
}

bool readAngle(float &outAngle) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_ADDR, 6, true) != 6) return false;

  int16_t ax = (Wire.read() << 8) | Wire.read();
  int16_t ay = (Wire.read() << 8) | Wire.read();
  int16_t az = (Wire.read() << 8) | Wire.read();
  float fx = ax / 16384.0, fy = ay / 16384.0, fz = az / 16384.0;

  if (TILT_AXIS == 0) outAngle = atan2(fx, sqrt(fy * fy + fz * fz)) * 180.0 / PI;
  else                outAngle = atan2(fy, sqrt(fx * fx + fz * fz)) * 180.0 / PI;
  return true;
}

// ---------------------- LED ----------------------
uint32_t colorForScore(float s) {
  return strip.Color((uint8_t)(255 * (1.0 - s)), (uint8_t)(255 * s), 0);
}

void showLeds() {
  float target = score * NUM_LEDS;
  displayLevel += (target - displayLevel) * 0.1;

  strip.clear();
  if (millis() < flashUntil) {
    for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, strip.Color(0, 0, 60));
  }
  int lit = (int)(displayLevel + 0.5);
  for (int i = 0; i < lit && i < NUM_LEDS; i++) {
    strip.setPixelColor(i, colorForScore((float)i / (NUM_LEDS - 1)));
  }
  strip.show();
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

// ---------------------- WiFi server (ESP32) ----------------------
#if USE_WIFI
void sendCors() { server.sendHeader("Access-Control-Allow-Origin", "*"); }

void handleData() {
  sendCors();
  server.send(200, "application/json", buildJson());
}

void handleReset() {
  strokeCount = 0; spm = 0; lastStrokeMs = 0; wasLow = false;
  sendCors();
  server.send(200, "application/json", "{\"ok\":true}");
}

void wifiSetup() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("{\"info\":\"pripojuji WiFi\"}\n");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(250);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("{\"info\":\"WiFi OK\",\"ip\":\"");
    Serial.print(WiFi.localIP());
    Serial.println("\"}");
  } else {
    Serial.println("{\"info\":\"WiFi se nepripojila\"}");
  }
  server.on("/data", handleData);
  server.on("/reset", handleReset);
  server.begin();
}
#endif

// ---------------------- Arduino ----------------------
void setup() {
  Serial.begin(115200);
  mpuInit();
  strip.begin();
  strip.setBrightness(LED_BRIGHTNESS);
  strip.show();
#if USE_WIFI
  wifiSetup();
#endif
}

void loop() {
#if USE_WIFI
  server.handleClient();
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
