// Rehabilitační tyč – hra dvou hráčů přes server (kutina_mauer)
// Tento soubor je pro TYČ 2. Soubor tyc1/tyc1.ino je stejný, liší se jen HRAC_ID.
//
// Deska: Arduino Nano RP2040 Connect
// Knihovny (Library Manager): NeoPixelConnect, Arduino_LSM6DSOX, WiFiNINA, ArduinoHttpClient

#include <NeoPixelConnect.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>

// ---------- NASTAVENÍ ----------
const char HRAC_ID[]   = "tyc2";
const char MISTNOST[]  = "rehab";           // stejná na obou tyčích i na webu
char ssid[] = "NAZEV_WIFI";                 // Wi-Fi, na které je i počítač se serverem
char pass[] = "HESLO_WIFI";
const char SERVER_IP[] = "192.168.1.100";   // IP počítače se serverem (vypíše ji server po spuštění)
const int SERVER_PORT  = 3000;

#define POCET_LED 64
#define SIRKA_CILE 10
#define VITEZNE_SKORE 10
const unsigned long DOBA_V_CILI  = 1500;    // ms – jak dlouho držet kuličku ve žlutém pásu
const unsigned long DOBA_OTOCENI = 3000;    // ms – jak dlouho držet otočenou tyč
const float PRAH_OTOCENI = -0.5;            // osa Z oproti poloze při zapnutí: pod prahem = tyč je otočená

NeoPixelConnect np(4, POCET_LED, pio0, 0);
WiFiClient wifi;
WebSocketClient ws(wifi, SERVER_IP, SERVER_PORT);

// ---------- STAV HRY ----------
enum Stav { CEKANI, HRA, KONEC };
Stav stav = CEKANI;          // posílá server: waiting / playing / finished
bool solo = false;           // sólo hra (připojená jen jedna tyč)
bool vyhral = false;
int skore = 0;
long epocha = -1;            // číslo hry ze serveru (-1 = zatím neznámé)

float x, y, z;
int zNormalne = 1;           // znaménko osy Z v normální poloze (zjistí se při zapnutí)
int pozice = 0;              // moje červená dioda
int cil = 0;                 // začátek mého žlutého pásu
int souperPozice = -1;       // modrá dioda soupeře (-1 = soupeř není vidět)
bool rezimSkore = false;

bool pripojeno = false;
unsigned long vCiliOd = 0, otocenaOd = 0, posledniPokus = 0, posledniOdeslani = 0;
int odeslanaPozice = -1, odeslanyCil = -1;

void setup() {
  Serial.begin(115200);
  delay(1500);
  np.neoPixelClear(true);

  if (!IMU.begin()) {
    Serial.println("Chyba IMU!");
    while (1);
  }

  // normální poloha = poloha při zapnutí (tyč leží LED nahoru), podle ní se pozná otočení
  unsigned long start = millis();
  while (!IMU.accelerationAvailable() && millis() - start < 1000);
  IMU.readAcceleration(x, y, z);
  zNormalne = (z >= 0) ? 1 : -1;
  Serial.print("Normalni poloha: z = ");
  Serial.println(z);

  randomSeed(analogRead(A0));
  novyCil();
}

void loop() {
  udrzujSpojeni();
  prijmiZpravy();

  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
  }
  pozice = constrain(mapFloat(x, -0.5, 0.5, 0, POCET_LED - 1), 0, POCET_LED - 1);

  hlidejOtoceni();
  if (stav == HRA && !rezimSkore) hraj();
  posliPozici();
  vykresli();
}

// ---------- HRA ----------

int mapFloat(float val, float in_min, float in_max, int out_min, int out_max) {
  return (val - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void novyCil() {
  do {
    cil = random(0, POCET_LED - SIRKA_CILE + 1);
  } while (pozice >= cil && pozice < cil + SIRKA_CILE);   // ne tam, kde už kulička je
  vCiliOd = 0;
}

// Kulička musí být v cíli bez přerušení DOBA_V_CILI => bod a nový cíl.
void hraj() {
  if (skore >= VITEZNE_SKORE) return;   // čekáme, až server vyhlásí konec
  bool vCili = pozice >= cil && pozice < cil + SIRKA_CILE;
  if (!vCili) {
    vCiliOd = 0;
    return;
  }
  if (vCiliOd == 0) vCiliOd = millis();
  if (millis() - vCiliOd >= DOBA_V_CILI) {
    skore++;
    Serial.print("Ukol splnen, skore: ");
    Serial.println(skore);
    posliSkore();
    novyCil();
  }
}

// Osa Z: tyč otočená a držená 3 s => režim skóre.
// Stejné gesto po konci hry spustí novou hru a při čekání na soupeře sólo hru.
void hlidejOtoceni() {
  if (z * zNormalne >= PRAH_OTOCENI) {
    otocenaOd = 0;
    rezimSkore = false;
    return;
  }
  if (otocenaOd == 0) otocenaOd = millis();
  if (millis() - otocenaOd < DOBA_OTOCENI) return;

  if (stav == KONEC) {
    posli("{\"type\":\"rematch\"}");
    otocenaOd = millis();   // případný další pokus až za 3 s
  } else if (stav == CEKANI) {
    posli("{\"type\":\"solo\"}");
    otocenaOd = millis();
  } else {
    rezimSkore = true;
  }
}

void vykresli() {
  bool blik = (millis() / 300) % 2;
  for (int i = 0; i < POCET_LED; i++) {
    uint8_t r = 0, g = 0, b = 0;
    if (stav == KONEC) {
      if (blik) {
        if (vyhral) g = 120;               // vítěz bliká zeleně
        else r = 120;                      // poražený červeně
      }
    } else if (rezimSkore) {
      if (i < skore) r = g = b = 60;       // bílé diody = počet splněných úkolů
    } else if (i == pozice) {
      r = 120;                             // já – červená
    } else if (i == souperPozice) {
      b = 120;                             // soupeř – modrá
    } else if (stav == HRA && i >= cil && i < cil + SIRKA_CILE) {
      r = 20; g = 20;                      // můj cíl – žlutá
    }
    np.neoPixelSetValue(i, r, g, b, false);
  }
  np.neoPixelShow();
}

// Během připojování (trvá i několik sekund) svítí první dioda modře.
void ukazPripojovani() {
  np.neoPixelClear(false);
  np.neoPixelSetValue(0, 0, 0, 60, false);
  np.neoPixelShow();
}

// ---------- KOMUNIKACE SE SERVEREM ----------

// Připojení k Wi-Fi a serveru. Při výpadku se každých 5 s zkouší znovu, hra mezitím běží offline.
void udrzujSpojeni() {
  if (pripojeno) {
    if (ws.connected() && WiFi.status() == WL_CONNECTED) return;
    Serial.println("Spojeni ztraceno, hraju offline");
    ws.stop();
    pripojeno = false;
    souperPozice = -1;
  }
  if (posledniPokus != 0 && millis() - posledniPokus < 5000) return;
  posledniPokus = millis();
  ukazPripojovani();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Pripojuji Wi-Fi ");
    Serial.println(ssid);
    if (WiFi.begin(ssid, pass) != WL_CONNECTED) {
      Serial.println("Wi-Fi se nepripojila - zkontroluj ssid a pass");
      return;
    }
    Serial.print("Wi-Fi OK, IP tyce: ");
    Serial.println(WiFi.localIP());
  }
  Serial.print("Pripojuji server ");
  Serial.print(SERVER_IP);
  Serial.print(":");
  Serial.println(SERVER_PORT);
  if (ws.begin() != 0) {
    ws.stop();
    Serial.println("Server nedostupny - bezi server (npm start)? Sedi SERVER_IP? Povolil jsi firewall?");
    return;
  }

  pripojeno = true;
  odeslanaPozice = -1;   // hned poslat aktuální polohu
  char zprava[80];
  snprintf(zprava, sizeof(zprava), "{\"type\":\"join\",\"room\":\"%s\",\"id\":\"%s\"}", MISTNOST, HRAC_ID);
  posli(zprava);
  Serial.println("Pripojeno k serveru");
}

bool posli(const char* text) {
  if (!pripojeno) return false;
  ws.beginMessage(TYPE_TEXT);
  ws.print(text);
  ws.endMessage();
  return true;
}

void posliSkore() {
  char zprava[64];
  snprintf(zprava, sizeof(zprava), "{\"type\":\"score\",\"score\":%d,\"epoch\":%ld}", skore, epocha);
  posli(zprava);
}

// Poloha se posílá max. 20× za sekundu a jen když se změnila.
void posliPozici() {
  if (millis() - posledniOdeslani < 50) return;
  if (pozice == odeslanaPozice && cil == odeslanyCil) return;
  char zprava[48];
  snprintf(zprava, sizeof(zprava), "{\"type\":\"pos\",\"pos\":%d,\"cil\":%d}", pozice, cil);
  if (posli(zprava)) {
    odeslanaPozice = pozice;
    odeslanyCil = cil;
    posledniOdeslani = millis();
  }
}

void prijmiZpravy() {
  if (!pripojeno) return;
  // parseMessage() zároveň sám odpovídá na ping serveru
  for (int i = 0; i < 5 && ws.parseMessage() > 0; i++) {
    char zprava[200];
    int n = 0;
    unsigned long start = millis();
    while (ws.available() > 0 && millis() - start < 100) {
      int c = ws.read();
      if (c >= 0 && n < (int)sizeof(zprava) - 1) zprava[n++] = c;
    }
    zprava[n] = '\0';
    zpracujZpravu(zprava);
  }
}

// Vytáhne číslo za klíčem, např. cislo(zprava, "\"pos\":")
long cislo(const char* zprava, const char* klic) {
  const char* p = strstr(zprava, klic);
  return p ? atol(p + strlen(klic)) : -1;
}

void zpracujZpravu(const char* zprava) {
  if (strstr(zprava, "\"type\":\"opp\"")) {
    souperPozice = cislo(zprava, "\"pos\":");

  } else if (strstr(zprava, "\"type\":\"status\"")) {
    if (strstr(zprava, "\"state\":\"playing\"")) stav = HRA;
    else if (strstr(zprava, "\"state\":\"finished\"")) stav = KONEC;
    else stav = CEKANI;
    solo = strstr(zprava, "\"solo\":true") != NULL;
    vyhral = strstr(zprava, "\"won\":true") != NULL;
    if (!strstr(zprava, "\"oppConnected\":true")) souperPozice = -1;

    long e = cislo(zprava, "\"epoch\":");
    int skoreNaServeru = cislo(zprava, "\"you\":");
    if (e != epocha) {
      if (epocha != -1) {        // server spustil novou hru
        skore = 0;
        novyCil();
        Serial.println("Nova hra");
      }
      epocha = e;
    }
    if (skoreNaServeru > skore) skore = skoreNaServeru;           // např. po restartu tyče
    if (skore > skoreNaServeru && stav == HRA) posliSkore();      // body získané během výpadku

    Serial.print("Stav: ");
    if (stav == HRA) Serial.print(solo ? "solo hra" : "duel");
    else if (stav == KONEC) Serial.print(vyhral ? "VYHRA" : "prohra");
    else Serial.print("cekam na soupere (otoc tyc na 3 s = solo hra)");
    Serial.print(", skore ");
    Serial.println(skore);

  } else if (strstr(zprava, "\"type\":\"full\"")) {
    Serial.println("Mistnost je plna - uz hraji dve jine tyce");
  } else if (strstr(zprava, "\"type\":\"kicked\"")) {
    Serial.println("Stejne HRAC_ID se pripojilo odjinud (jina tyc nebo okno prohlizece)");
  }
}
