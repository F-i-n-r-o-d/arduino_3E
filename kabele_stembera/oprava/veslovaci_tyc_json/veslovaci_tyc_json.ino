/*
  Rehabilitační veslařská tyč - jen USB (Arduino Nano RP2040 Connect)

  - LSM6DSOX měří náklon, detekce záběrů, tempo
  - LED pásek: jen modré bliknutí při každém záběru
  - Přes USB (Serial) se každých 100 ms posílá 1 řádek JSON
  - Příkaz 'r' přijatý přes USB vynuluje počítadlo záběrů

  Knihovny: NeoPixelConnect, Arduino_LSM6DSOX
*/

#include <NeoPixelConnect.h>
#include <Arduino_LSM6DSOX.h>
#include <math.h>

// ====================== NASTAVENÍ ======================
#define LED_PIN        4
#define NUM_LEDS       64
#define LED_BRIGHTNESS 80      // Jas 0-255

const float ANGLE_HIGH = 15.0;
const float ANGLE_LOW  = -15.0;
const unsigned long MIN_STROKE_MS = 1000;
const unsigned long TIMEOUT_MS    = 5000;

const float TARGET_SPM    = 22.0;
const float TOLERANCE_SPM = 10.0;

#define TILT_AXIS 0            // 0 = osa X, 1 = osa Y
// =======================================================

NeoPixelConnect strip(LED_PIN, NUM_LEDS, pio0, 0);

float angle = 0;
bool  wasLow = false;
unsigned long lastStrokeMs = 0;
unsigned long strokeCount  = 0;
float spm = 0;          // tempo naměřené při posledním záběru
float liveSpm = 0;      // tempo v reálném čase (klesá, když se tyč zastaví)
float score = 0;
unsigned long flashUntil = 0;
unsigned long lastJson = 0;
bool ledsWereOn = false;

// ---------------------- JSON ----------------------
String buildJson() {
  String j;
  j.reserve(140);
  j  = "{\"t\":";         j += millis();
  j += ",\"angle\":";     j += String(angle, 1);
  j += ",\"strokes\":";   j += strokeCount;
  j += ",\"spm\":";       j += String(liveSpm, 1);
  j += ",\"target\":";    j += String(TARGET_SPM, 0);
  j += ",\"score\":";     j += String(score, 2);
  j += "}";
  return j;
}

// ---------------------- Akcelerometr ----------------------
bool readAngle(float &outAngle) {
  if (!IMU.accelerationAvailable()) return false;

  float fx, fy, fz;
  if (!IMU.readAcceleration(fx, fy, fz)) return false;

  if (TILT_AXIS == 0) outAngle = atan2(fx, sqrt(fy * fy + fz * fz)) * 180.0 / PI;
  else                outAngle = atan2(fy, sqrt(fx * fx + fz * fz)) * 180.0 / PI;
  return true;
}

// ---------------------- LED: jen modré bliknutí ----------------------
void updateLeds(unsigned long now) {
  bool on = now < flashUntil;
  if (on) {
    uint8_t b = (60 * LED_BRIGHTNESS) / 255;
    for (int i = 0; i < NUM_LEDS; i++) {
      strip.neoPixelSetValue((uint8_t)i, 0, 0, b, false);
    }
    strip.neoPixelShow();
  } else if (ledsWereOn) {
    strip.neoPixelClear(true);   // zhasnout jen jednou po skončení bliknutí
  }
  ledsWereOn = on;
}

// ---------------------- Detekce záběru ----------------------
void onStroke(unsigned long now) {
  strokeCount++;
  flashUntil = now + 120;
  if (lastStrokeMs != 0) {
    float instSpm = 60.0 / ((now - lastStrokeMs) / 1000.0);
    spm = (spm == 0) ? instSpm : (0.5 * spm + 0.5 * instSpm);
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

// Tempo v reálném čase: když záběr trvá déle, než odpovídá poslednímu tempu,
// tempo plynule klesá.
void updateLiveSpm(unsigned long now) {
  if (spm <= 0 || lastStrokeMs == 0) { liveSpm = 0; return; }
  float sinceMs    = (float)(now - lastStrokeMs);
  float expectedMs = 60000.0 / spm;
  liveSpm = (sinceMs > expectedMs) ? 60000.0 / sinceMs : spm;
}

void resetAll() {
  strokeCount = 0;
  spm = 0; liveSpm = 0; score = 0;
  lastStrokeMs = 0;
  wasLow = false;
}

// ---------------------- Příkazy z USB ----------------------
void handleSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'r' || c == 'R') resetAll();
  }
}

// ---------------------- Arduino ----------------------
void setup() {
  Serial.begin(115200);
  delay(1500);

  if (!IMU.begin()) {
    Serial.println("{\"error\":\"IMU nenalezeno\"}");
  }

  strip.neoPixelClear(true);
}

void loop() {
  handleSerialCommands();

  static unsigned long lastSample = 0;
  unsigned long now = millis();
  if (now - lastSample < 10) return;
  lastSample = now;

  float raw;
  if (readAngle(raw)) {
    angle = 0.8 * angle + 0.2 * raw;
    detectStroke(now);
  }

  updateLiveSpm(now);
  score = 0;
  if (liveSpm > 0) {
    score = 1.0 - fabs(liveSpm - TARGET_SPM) / TOLERANCE_SPM;
    score = constrain(score, 0.0, 1.0);
  }

  updateLeds(now);

  if (now - lastJson >= 100) {
    lastJson = now;
    Serial.println(buildJson());
  }
}