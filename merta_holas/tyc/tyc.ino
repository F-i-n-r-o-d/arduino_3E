// Rehabilitační LED tyč — firmware tyče (kostra)
// Deska: Arduino Nano RP2040 Connect
//
// Tyč se připojí k WiFi, zaregistruje se na PC (PC jí přidělí roli),
// pak se v cyklu ptá PC na příkaz (start/stop). Během záznamu ukazuje
// na pásku kuličku podle náklonu a ukládá vzorky do paměti.
// Po stopu pošle celý záznam na PC jako JSON.

#include <NeoPixelConnect.h>   // LED pásek
#include <Arduino_LSM6DSOX.h>  // akcelerometr
#include <WiFiNINA.h>
#include <SPI.h>

// ---------- nastavení ----------
char ssid[] = "RehabSit";        // WiFi síť, ve které je i PC
char pass[] = "heslo1234";
IPAddress pcIp(192, 168, 1, 100); // IP adresa PC se serverem
const int PC_PORT = 8000;

#define POCET_LED 64
#define LED_PIN 4
#define SIRKA_KULICKY 5          // kolik sousedních diod tvoří kuličku
#define MAX_UHEL 60.0            // krajní úhel (°) odpovídající konci tyče
#define SMER 1                   // když jde kulička k nižšímu konci místo k vyššímu, změň na -1

#define PERIODA_VZORKU_MS 50     // 20 Hz
#define PERIODA_DOTAZU_MS 500    // jak často se ptát PC na příkaz
#define MAX_VZORKU 1200          // 60 s záznamu při 20 Hz

NeoPixelConnect np(LED_PIN, POCET_LED, pio0, 0);

// ---------- stav ----------
enum Stav { CEKANI, ZAZNAM, ODESILANI, HOTOVO };
Stav stav = CEKANI;

String idTyce;          // z MAC adresy, aby obě tyče mohly mít stejný kód
String role = "?";      // "mentor" / "pacient", přidělí PC

struct Vzorek {
  unsigned long t;      // ms od začátku záznamu
  float uhel;           // stupně
};
Vzorek vzorky[MAX_VZORKU];
int pocetVzorku = 0;

unsigned long startZaznamu = 0;
unsigned long posledniVzorek = 0;
unsigned long posledniDotaz = 0;
unsigned long hotovoOd = 0;

// ---------- pomocné funkce ----------

// Náklon tyče vůči vodorovné poloze ve stupních. Tyč leží vodorovně na rukách
// a naklání se jako houpačka, osa X desky vede podél tyče.
// Kladný úhel = konec s diodou 63 je výš. Kulička funguje jako bublinka
// ve vodováze — jde k vyššímu konci.
// TODO: ověřit na skutečné tyči podle toho, jak bude deska a pásek otočený (SMER).
float ctiUhel() {
  static float uhel = 0;
  float x, y, z;
  if (IMU.accelerationAvailable()) {
    IMU.readAcceleration(x, y, z);
    uhel = atan2(SMER * x, sqrt(y * y + z * z)) * 180.0 / PI;
  }
  return uhel;
}

void vyplnPasek(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < POCET_LED; i++) {
    np.neoPixelSetValue((uint8_t)i, r, g, b, false);
  }
  np.neoPixelShow();
}

void zobrazKulicku(float uhel) {
  int stred = map((long)(uhel * 10), (long)(-MAX_UHEL * 10), (long)(MAX_UHEL * 10), 0, POCET_LED - 1);
  stred = constrain(stred, 0, POCET_LED - 1);
  for (int i = 0; i < POCET_LED; i++) {
    if (abs(i - stred) <= SIRKA_KULICKY / 2) np.neoPixelSetValue((uint8_t)i, 120, 0, 0, false);
    else np.neoPixelSetValue((uint8_t)i, 0, 0, 0, false);
  }
  np.neoPixelShow();
}

// Připojí se k PC a pošle hlavičku HTTP požadavku. Tělo se pak posílá zvlášť.
bool posliHlavicku(WiFiClient& klient, const char* metoda, String cesta, long delkaTela) {
  if (!klient.connect(pcIp, PC_PORT)) {
    Serial.println("Nelze se pripojit k PC");
    return false;
  }
  String h = String(metoda) + " " + cesta + " HTTP/1.1\r\nHost: pc\r\nConnection: close\r\n";
  if (delkaTela > 0) {
    h += "Content-Type: application/json\r\nContent-Length: " + String(delkaTela) + "\r\n";
  }
  klient.print(h + "\r\n");
  return true;
}

// Přečte odpověď a vrátí její tělo (nebo "" při chybě).
String prectiOdpoved(WiFiClient& klient) {
  String odpoved = "";
  unsigned long zacatek = millis();
  while ((klient.connected() || klient.available()) && millis() - zacatek < 2000) {
    while (klient.available()) odpoved += (char)klient.read();
  }
  klient.stop();

  int konecHlavicky = odpoved.indexOf("\r\n\r\n");
  if (konecHlavicky < 0) return "";
  return odpoved.substring(konecHlavicky + 4);
}

// Jednoduchý HTTP požadavek na PC. Vrací tělo odpovědi (nebo "" při chybě).
String httpPozadavek(const char* metoda, String cesta, String telo) {
  WiFiClient klient;
  if (!posliHlavicku(klient, metoda, cesta, telo.length())) return "";
  if (telo.length() > 0) klient.print(telo);
  return prectiOdpoved(klient);
}

// Vytáhne hodnotu textového klíče z jednoduchého JSONu {"klic":"hodnota"}
String jsonHodnota(String json, String klic) {
  int i = json.indexOf("\"" + klic + "\"");
  if (i < 0) return "";
  i = json.indexOf(':', i);
  int a = json.indexOf('"', i + 1);
  int b = json.indexOf('"', a + 1);
  if (a < 0 || b < 0) return "";
  return json.substring(a + 1, b);
}

void pripojWifi() {
  Serial.print("Pripojuji k WiFi ");
  Serial.println(ssid);
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    vyplnPasek(20, 0, 0); // červená = bez WiFi
    delay(2000);
  }
  Serial.print("Pripojeno, IP: ");
  Serial.println(WiFi.localIP());
}

void registrace() {
  String odpoved = httpPozadavek("POST", "/api/registrace", "{\"id\":\"" + idTyce + "\"}");
  String r = jsonHodnota(odpoved, "role");
  if (r.length() > 0) role = r;
  Serial.println("Role: " + role);
}

// Zeptá se PC, co má tyč dělat. Odpověď třeba {"prikaz":"start","role":"mentor"}
String dotazNaPrikaz() {
  String odpoved = httpPozadavek("GET", "/api/prikaz?id=" + idTyce, "");
  String r = jsonHodnota(odpoved, "role");
  if (r.length() > 0) role = r;
  return jsonHodnota(odpoved, "prikaz");
}

String vzorekJson(int i) {
  return String(i > 0 ? "," : "") + "{\"t\":" + String(vzorky[i].t) + ",\"uhel\":" + String(vzorky[i].uhel, 1) + "}";
}

// Pošle celý záznam. Celý JSON by měl i 30 kB, takže se neskládá do jednoho
// Stringu, ale posílá se po kusech (délka se spočítá předem kvůli hlavičce).
// TODO: odesílat průběžně už během záznamu, ať krátký výpadek WiFi nevadí
bool odesliZaznam() {
  String zacatek = "{\"id\":\"" + idTyce + "\",\"role\":\"" + role + "\",\"vzorky\":[";
  String konec = "]}";
  long delka = zacatek.length() + konec.length();
  for (int i = 0; i < pocetVzorku; i++) delka += vzorekJson(i).length();

  WiFiClient klient;
  if (!posliHlavicku(klient, "POST", "/api/data", delka)) return false;
  String kus = zacatek;
  for (int i = 0; i < pocetVzorku; i++) {
    kus += vzorekJson(i);
    if (kus.length() > 1000) {
      klient.print(kus);
      kus = "";
    }
  }
  klient.print(kus + konec);

  String odpoved = prectiOdpoved(klient);
  Serial.println("Odeslano " + String(pocetVzorku) + " vzorku, odpoved: " + odpoved);
  return odpoved.length() > 0; // opakovat jen při chybě spojení
}

// ---------- hlavní program ----------

void setup() {
  Serial.begin(115200);
  delay(1500);

  if (!IMU.begin()) {
    Serial.println("Chyba IMU!");
    while (1);
  }

  pripojWifi();

  byte mac[6];
  WiFi.macAddress(mac);
  char buf[13];
  sprintf(buf, "%02X%02X%02X", mac[2], mac[1], mac[0]);
  idTyce = String("tyc-") + buf;
  Serial.println("ID tyce: " + idTyce);

  registrace();
}

void loop() {
  unsigned long ted = millis();

  if (WiFi.status() != WL_CONNECTED) {
    pripojWifi();
  }

  switch (stav) {
    case CEKANI: {
      // modrá na kraji = čekám na start (mentor má modrou dole, pacient nahoře)
      vyplnPasek(0, 0, 0);
      int led = (role == "pacient") ? POCET_LED - 1 : 0;
      np.neoPixelSetValue((uint8_t)led, 0, 0, 60, true);

      if (ted - posledniDotaz > PERIODA_DOTAZU_MS) {
        posledniDotaz = ted;
        // PC tyč nezná (při startu tyče neběželo nebo se restartovalo)
        if (role == "?") registrace();
        if (dotazNaPrikaz() == "start") {
          Serial.println("START zaznamu");
          pocetVzorku = 0;
          startZaznamu = millis();
          stav = ZAZNAM;
        }
      }
      break;
    }

    case ZAZNAM: {
      if (ted - posledniVzorek >= PERIODA_VZORKU_MS) {
        posledniVzorek = ted;
        float uhel = ctiUhel();
        zobrazKulicku(uhel);
        if (pocetVzorku < MAX_VZORKU) {
          vzorky[pocetVzorku].t = ted - startZaznamu;
          vzorky[pocetVzorku].uhel = uhel;
          pocetVzorku++;
        }
      }
      if (ted - posledniDotaz > PERIODA_DOTAZU_MS) {
        posledniDotaz = ted;
        if (dotazNaPrikaz() == "stop" || pocetVzorku >= MAX_VZORKU) {
          Serial.println("STOP zaznamu");
          stav = ODESILANI;
        }
      }
      break;
    }

    case ODESILANI:
      vyplnPasek(30, 30, 0); // žlutá = odesílám
      for (int pokus = 0; pokus < 5; pokus++) {
        if (odesliZaznam()) break;
        delay(1000);
      }
      hotovoOd = millis();
      stav = HOTOVO;
      break;

    case HOTOVO:
      vyplnPasek(0, 40, 0); // zelená = hotovo
      if (ted - hotovoOd > 2000) stav = CEKANI;
      break;
  }

  delay(5);
}
