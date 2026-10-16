#include <NeoPixelConnect.h> // knihovna pro led pásku
// #include <Arduino_LSM6DSOX.h> // knihovna pro čtení náklonu
#include <SPI.h>

#define MAXIMUM_NUM_NEOPIXELS 64 // Nastavení délky pásku na 64 diod
#define LED_PIN 4

NeoPixelConnect np(LED_PIN, MAXIMUM_NUM_NEOPIXELS, pio0, 0); // Inicializace pásku na digitálním pinu D4
int i;
int randNumber;
int citac = 0; // Počítadlo času stráveného v cíli
float x, y, z; // Proměnné pro ukládání hodnot zrychlení/náklonu
char ssid[] = "RP2040";        // your network SSID (name)
char pass[] = "ArduinoWeb";    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;
int chargePercent = 65;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  // Визначаємо колір індикатора
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

  // Обчислюємо кількість увімкнених LED
  int litLeds = chargePercent * MAXIMUM_NUM_NEOPIXELS / 100;

  // Оновлюємо всі 64 світлодіоди
  for (int i = 0; i < MAXIMUM_NUM_NEOPIXELS; i++) {
    if (i < litLeds) {
      np.neoPixelSetValue(i, red, green, 0, true);
    } else {
      np.neoPixelSetValue(i, 0, 0, 0, true);
    }
  }

  Serial.print("Battery level: ");
  Serial.print(chargePercent);
  Serial.println("%");
}

void loop() {
  // put your main code here, to run repeatedly:

}
