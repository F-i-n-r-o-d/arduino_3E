//#include <WiFi.h>
//#include <FastLED.h>
 
const char* ssid = "";
const char* password = "";
 
#define LED_PIN     23
#define NUM_LEDS    64
#define BRIGHTNESS  50
 
CRGB leds[NUM_LEDS];
 
int skore = 0;
 
void setup() {
  Serial.begin(115200);
 
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
 
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi připojena, připraveno na kód hry.");
}
 
void loop() {
 
  FastLED.clear();
  leds[0] = CRGB::Blue;
  FastLED.show();
  delay(100);
}