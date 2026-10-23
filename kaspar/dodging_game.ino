#include <Adafruit_NeoPixel.h>

#define LED_PIN 6
#define LED_COUNT 64

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

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

void setup() {
  strip.begin();
  strip.show();

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
}

void generateObstacle() {

  // Bezpečné zóny 7-9 LED
  safeSize1 = random(7, 10);
  safeStart1 = random(0, 25);

  safeSize2 = random(7, 10);
  safeStart2 = random(35, 55);
}

void drawGame() {

  strip.clear();

  for (int i = 0; i < LED_COUNT; i++) {

    bool safeZone = false;

    if (i >= safeStart1 && i < safeStart1 + safeSize1) {
      safeZone = true;
    }

    if (i >= safeStart2 && i < safeStart2 + safeSize2) {
      safeZone = true;
    }

    if (safeZone) {
      strip.setPixelColor(i, strip.Color(0, 120, 0));   // zelená
    } else {
      strip.setPixelColor(i, strip.Color(255, 80, 0));  // oranžová
    }
  }

  // Hráč
  for (int i = 0; i < playerSize; i++) {
    strip.setPixelColor(playerPos + i, strip.Color(0, 0, 255)); // modrá
  }

  // Životy
  for (int i = 0; i < lives; i++) {
    strip.setPixelColor(i, strip.Color(255, 255, 255)); // bílá
  }

  strip.show();
}

void loseLifeEffect() {

  for (int blink = 0; blink < 3; blink++) {

    // Rozsviť vše červeně
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, strip.Color(255, 0, 0));
    }

    strip.show();
    delay(150);

    // Zhasni vše
    strip.clear();
    strip.show();
    delay(150);
  }
}

void gameOver() {

  while (true) {

    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, strip.Color(255, 0, 0));
    }

    strip.show();
    delay(100);
  }
}