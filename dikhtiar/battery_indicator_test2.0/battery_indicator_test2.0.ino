#include <NeoPixelConnect.h>

#define LED_COUNT 64
#define LED_PIN 4

NeoPixelConnect np(LED_PIN, LED_COUNT, pio0, 0);

void setup() {
  for (int i = 0; i < LED_COUNT; i++) {
    np.neoPixelSetValue(i, 0, 120, 0, false);
  }

  np.neoPixelShow();
}

void loop() {
}