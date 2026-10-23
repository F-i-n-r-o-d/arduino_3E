#include <NeoPixelConnect.h>

#define LED_COUNT 64
#define LED_PIN 4  // GPIO 4 = D12 на Nano RP2040 Connect

NeoPixelConnect np(LED_PIN, LED_COUNT, pio0, 0);

int chargePercent = 65;

void setup() {
  Serial.begin(115200);

  // Обмежуємо значення діапазоном 0–100%
  chargePercent = constrain(chargePercent, 0, 100);

  int red = 0;
  int green = 0;

  if (chargePercent <= 20) {
    red = 120;
  } else if (chargePercent <= 50) {
    red = 120;
    green = 70;
  } else {
    green = 120;
  }

  int litLeds = chargePercent * LED_COUNT / 100;

  for (int i = 0; i < LED_COUNT; i++) {
    if (i < litLeds) {
      np.neoPixelSetValue(i, red, green, 0, false);
    } else {
      np.neoPixelSetValue(i, 0, 0, 0, false);
    }
  }

  // Передаємо підготовлені значення на стрічку
  np.neoPixelShow();

  Serial.print("Battery level: ");
  Serial.print(chargePercent);
  Serial.println("%");
}

void loop() {
}