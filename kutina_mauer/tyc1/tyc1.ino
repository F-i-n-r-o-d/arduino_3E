// Rehabilitační tyč – hra dvou hráčů přes server (kutina_mauer)
// Tento soubor je pro TYČ 1. Soubor tyc2/tyc2.ino je stejný, liší se jen HRAC_ID.
//
// Deska: Arduino Nano RP2040 Connect
// Knihovny (Library Manager): NeoPixelConnect, Arduino_LSM6DSOX, WiFiNINA, ArduinoHttpClient

#include <NeoPixelConnect.h>
#include <Arduino_LSM6DSOX.h>
#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>

// ---------- NASTAVENÍ ----------
const char HRAC_ID[]   = "tyc1";
const char MISTNOST[]  = "rehab";           // stejná na obou tyčích i na webu
char ssid[] = "NAZEV_WIFI";                 // Wi-Fi, na které je i počítač se serverem
char pass[] = "HESLO_WIFI";
const char SERVER_IP[] = "192.168.1.100";   // IP počítače se serverem (vypíše ji server po spuštění)
const int SERVER_PORT  = 3000;

#define POCET_LED 64
#define SIRKA_CILE 10
#define VITEZNE_SKORE 10
const unsigned long DOBA_V_CILI  = 1500;    // ms – jak dlouho držet kuličku ve žlutém pásu
const float VYHLAZENI = 0.12;               // 0–1: menší = plynulejší pohyb, větší = rychlejší reakce
const float PRAH_TRESENI = 2.0;             // g – tak silný náraz se počítá do zatřesení
const int POCET_NARAZU = 3;                 // kolik nárazů rychle po sobě = zatřesení
const unsigned long OKNO_TRESENI = 1000;    // ms – během jak dlouhé doby

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
float poziceF = 31.5;        // vyhlazená poloha kuličky – může být i mezi dvěma diodami
int pozice = 31;             // moje červená dioda (celé číslo pro hru a server)
int cil = 0;                 // začátek mého žlutého pásu
int souperPozice = -1;       // modrá dioda soupeře (-1 = soupeř není vidět)
bool rezimSkore = false;

bool pripojeno = false;
unsigned long vCiliOd = 0, posledniPokus = 0, posledniOdeslani = 0;
int odeslanaPozice = -1, odeslanyCil = -1;

int narazy = 0;              // počítadlo nárazů pro zatřesení
bool nadPrahem = false;
unsigned long prvniNaraz = 0, posledniTreseni = 0;

void setup() {
  Serial.begin(115200);
  delay(1500);
  np.neoPixelClear(true);

  if (!IMU.begin()) {
    Serial.println("Chyba IMU!");
    while (1);
  }

  randomSeed(analogRead(A0));
  novyCil();
}

void loop() {
  udrzujSpojeni();
  prijmiZpravy();

  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
    // náklon -0.5 g až +0.5 g => dioda 0 až 63, vyhlazeno, aby se třes ruky nepřenášel na diodu
    float cilovaPozice = constrain((x + 0.5) * (POCET_LED - 1), 0, POCET_LED - 1);
    hlidejZatreseni();
    if (!nadPrahem) poziceF += (cilovaPozice - poziceF) * VYHLAZENI;   // při nárazu (třesení) kulička stojí
  }
  // celá dioda pro hru: mění se až o kus dál, aby na hranici dvou diod neblikala tam a zpět
  if (fabs(poziceF - pozice) > 0.7) pozice = round(poziceF);

  if (stav == HRA && !rezimSkore) hraj();
  posliPozici();
  vykresli();
}

// ---------- HRA ----------

void novyCil() {
  do {
    cil = random(0, POCET_LED - SIRKA_CILE + 1);
  } while (pozice >= cil && pozice < cil + SIRKA_CILE);   // ne tam, kde už kulička je
  vCiliOd = 0;
}

// Kulička musí být v cíli bez přerušení DOBA_V_CILI => bod a nový cíl.
void hraj() {
  if (skore >= VITEZNE_SKORE) return;   // čekáme, až server vyhlásí konec
  bool vCili = poziceF >= cil - 0.5 && poziceF < cil + SIRKA_CILE - 0.5;   // stejně, jak je kulička vidět
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

// Zatřesení = POCET_NARAZU silných nárazů (celkové zrychlení nad PRAH_TRESENI) během OKNO_TRESENI.
// Běžné naklánění má kolem 1 g, takže se s tím nesplete.
void hlidejZatreseni() {
  float a = sqrt(x * x + y * y + z * z);
  if (nadPrahem) {
    if (a < 1.3) nadPrahem = false;        // náraz skončil, čekáme na další
    return;
  }
  if (a < PRAH_TRESENI) return;
  nadPrahem = true;
  if (narazy == 0 || millis() - prvniNaraz > OKNO_TRESENI) {
    narazy = 0;
    prvniNaraz = millis();
  }
  narazy++;
  if (narazy < POCET_NARAZU) return;
  narazy = 0;
  if (millis() - posledniTreseni < 1500) return;   // jedno zatřesení se nepočítá dvakrát
  posledniTreseni = millis();
  zatreseni();
}

// Zatřesení přepíná režim skóre (zůstane, dokud znovu nezatřeseš).
// Po konci hry spustí novou hru, při čekání na soupeře sólo hru.
void zatreseni() {
  Serial.println("Zatreseni!");
  if (stav == KONEC) {
    posli("{\"type\":\"rematch\"}");
  } else if (stav == CEKANI) {
    posli("{\"type\":\"solo\"}");
  } else {
    rezimSkore = !rezimSkore;
    vCiliOd = 0;                           // čas v cíli se počítá znovu
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
    } else {
      if (i == souperPozice) {
        b = 120;                           // soupeř – modrá
      } else if (stav == HRA && i >= cil && i < cil + SIRKA_CILE) {
        r = 20; g = 20;                    // můj cíl – žlutá
      }
      // moje kulička: svítí na dvou sousedních diodách podle přesné polohy => plynulý pohyb
      float k = 1.0 - fabs(i - poziceF);
      if (k > 0) {
        k = sqrt(k);                       // i mezi dvěma diodami svítí kulička stejně jasně
        r = r * (1 - k) + 120 * k;
        g = g * (1 - k);
        b = b * (1 - k);
      }
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
    if (stav != HRA) rezimSkore = false;
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
    else Serial.print("cekam na soupere (zatres tyci = solo hra)");
    Serial.print(", skore ");
    Serial.println(skore);

  } else if (strstr(zprava, "\"type\":\"full\"")) {
    Serial.println("Mistnost je plna - uz hraji dve jine tyce");
  } else if (strstr(zprava, "\"type\":\"kicked\"")) {
    Serial.println("Stejne HRAC_ID se pripojilo odjinud (jina tyc nebo okno prohlizece)");
  }
}
