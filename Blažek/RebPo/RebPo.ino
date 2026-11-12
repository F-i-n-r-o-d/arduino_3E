/*
 * =====================================================================================
 * Projekt: RehabRotator - Rehabilitační Pomůcka s Měřením Rotace
 * Hardware: Arduino Uno, MPU6050 Gyroskop, 64x WS2812B LED Pás, Wi-Fi Modul
 * Popis: Čtenie úhlu rotace, řízení 64 LED (středový box 28-36) a odesílání dat.
 * =====================================================================================
 */

#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <MPU6050_tockn.h>

// --- PINOUT A KONFIGURACE ---
#define LED_PIN          6       // Datový pin pro WS2812B
#define NUM_LEDS         64      // Celkový počet diod
#define CENTER_BOX_START 28      // Začátek středového boxu s elektronikou
#define CENTER_BOX_END   36      // Konec středového boxu s elektronikou

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
MPU6050 mpu6050(Wire);

float currentAngle = 0.0;
int activeLedsCount = 0;
unsigned long lastSendTime = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  mpu6050.begin();
  mpu6050.calcGyroOffsets(true); // Kalibrace při startu

  strip.begin();
  strip.show(); // Zhasnout LED
  strip.setBrightness(120);

  Serial.println("RehabRotator Arduino Firmware Inicializován.");
}

void loop() {
  mpu6050.update();
  currentAngle = abs(mpu6050.getAngleZ());

  if (currentAngle > 64.0) currentAngle = 64.0;

  // Přepočet úhlu na počet rozsvícených LED (1 stupeň = 1 LED)
  activeLedsCount = map((int)currentAngle, 0, 64, 0, NUM_LEDS);

  // Aktualizace LED pásu
  updateLedStrip(activeLedsCount);

  // Odeslání telemetrie každých 200ms
  if (millis() - lastSendTime > 200) {
    sendTelemetryJSON(currentAngle, activeLedsCount);
    lastSendTime = millis();
  }
}

void updateLedStrip(int count) {
  for (int i = 0; i < NUM_LEDS; i++) {
    if (i < count) {
      if (count >= NUM_LEDS) {
        strip.setPixelColor(i, strip.Color(16, 185, 129)); // Zelená (Max)
      } else if (i >= CENTER_BOX_START && i <= CENTER_BOX_END) {
        strip.setPixelColor(i, strip.Color(245, 158, 11)); // Jantarová (Box)
      } else {
        strip.setPixelColor(i, strip.Color(0, 240, 255));  // Azurová
      }
    } else {
      strip.setPixelColor(i, strip.Color(0, 0, 0)); // Zhasnuto
    }
  }
  strip.show();
}

void sendTelemetryJSON(float angle, int leds) {
  int progress = (int)((angle / 64.0) * 100.0);
  
  String json = "{";
  json += "\"angle\":" + String(angle, 1) + ",";
  json += "\"leds\":" + String(leds) + ",";
  json += "\"progress\":" + String(progress);
  json += "}";

  Serial.println(json);
}