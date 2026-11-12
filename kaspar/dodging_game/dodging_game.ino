#include <NeoPixelConnect.h>
#include <WiFiNINA.h>

#define LED_PIN 4
#define LED_COUNT 64

// Inicializace pásku na pinu 6 přes PIO0
NeoPixelConnect strip(LED_PIN, LED_COUNT, pio0, 0);

const int sensorPin = A0;

int lives = 3;

const int playerSize = 5;
int playerPos = 30;

int safeStart1;
int safeSize1;

int safeStart2;
int safeSize2;

unsigned long lastRound = 0;
int roundDelay = 3000;

void generateObstacle();
void drawGame();
void loseLifeEffect();
void gameOver();

void setup() {
  randomSeed(analogRead(A5));
  generateObstacle();
}

void loop() {
  int sensor = analogRead(sensorPin);

  // Naklonění doprava
  if (sensor > 600) {
    playerPos++;
  }

  // Naklonění doleva
  if (sensor < 400) {
    playerPos--;
  }

  playerPos = constrain(playerPos, 0, LED_COUNT - playerSize);

  drawGame();

  // Opraveno odečítání času
  if (millis() - lastRound > roundDelay) {
    bool safe = true;

    for (int i = 0; i < playerSize; i++) {
      int led = playerPos + i;

      bool inSafeZone =
        ((led >= safeStart1 && led < safeStart1 + safeSize1) ||
         (led >= safeStart2 && led < safeStart2 + safeSize2));

      if (!inSafeZone) {
        safe = false;
        break;
      }
    }

    if (!safe) {
      lives--;
      loseLifeEffect();
    }

    if (lives <= 0) {
      gameOver();
    }

    generateObstacle();

    // Postupné zrychlování
    if (roundDelay > 800) {
      roundDelay -= 50;
    }

    lastRound = millis();
  }

  delay(20); // Drobná prodleva pro plynulejší snímání analogového pinu
}

void generateObstacle() {
  safeSize1 = random(7, 10);
  safeStart1 = random(0, 25);

  safeSize2 = random(7, 10);
  safeStart2 = random(35, 55);
}

void drawGame() {
  strip.neoPixelClear(false);

  for (int i = 0; i < LED_COUNT; i++) {
    bool safeZone = false;

    if (i >= safeStart1 && i < safeStart1 + safeSize1) {
      safeZone = true;
    }

    if (i >= safeStart2 && i < safeStart2 + safeSize2) {
      safeZone = true;
    }

    if (safeZone) {
      strip.neoPixelSetValue(i, 0, 120, 0, false);   // zelená
    } else {
      strip.neoPixelSetValue(i, 255, 80, 0, false);  // oranžová
    }
  }

  // Hráč
  for (int i = 0; i < playerSize; i++) {
    strip.neoPixelSetValue(playerPos + i, 0, 0, 255, false); // modrá
  }

  // Životy
  for (int i = 0; i < lives; i++) {
    strip.neoPixelSetValue(i, 255, 255, 255, false); // bílá
  }

  strip.neoPixelShow();
}

void loseLifeEffect() {
  for (int blink = 0; blink < 3; blink++) {
    strip.neoPixelFill(255, 0, 0, true);
    delay(150);
    strip.neoPixelClear(true);
    delay(150);
  }
}

void gameOver() {
  while (true) {
    strip.neoPixelFill(255, 0, 0, true);
    delay(100);
  }
}