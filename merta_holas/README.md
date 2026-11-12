# Rehabilitační LED tyč — kostra projektu

Popis systému je ve [specifikace_projektu.md](specifikace_projektu.md).

## Struktura

```
tyc/tyc.ino            firmware tyče (Arduino Nano RP2040 Connect)
pc/server.py           PC server: Web API, přidělení rolí, porovnání, historie
pc/web/index.html      webová stránka pro mentora (ovládání, graf, historie)
pc/virtualni_tyc.py    virtuální tyč v prohlížeči, hýbe se myší (pc/web/tyc.html)
pc/simulator_tyce.py   automatická simulovaná tyč (hýbe se sama, sinusovka)
pc/data/historie.json  uložená sezení (vznikne samo)
```

## Jak to spustit

1. **Server na PC** (stačí Python 3, nic se neinstaluje):
   ```
   cd pc
   python server.py
   ```
   Pak otevřít http://localhost:8000

2. **Tyče** — v `tyc/tyc.ino` nastavit `ssid`, `pass` a `pcIp` (IP adresa PC
   ve stejné síti), nahrát do obou tyčí. Knihovny: `NeoPixelConnect`,
   `Arduino_LSM6DSOX`, `WiFiNINA`.

   **Bez hardwaru — virtuální tyč**, se kterou hýbeš myší (každá v jiném terminálu):
   ```
   python virtualni_tyc.py 8001
   python virtualni_tyc.py 8002
   ```
   Otevřít http://localhost:8001 a http://localhost:8002 — **každou v samostatném
   okně vedle sebe, ne v kartách**. Prohlížeč karty na pozadí brzdí, takže
   by tyč v neaktivní kartě nahrávala jen pár vzorků. Tyč leží vodorovně
   na rukách, nakloníš ji tažením za konec myší (nebo šipkami) a svítí stejně
   jako skutečná — kulička jde jako bublinka ve vodováze k vyššímu konci.

   Případně automatický simulátor, který se hýbe sám:
   ```
   python simulator_tyce.py tyc-sim1
   python simulator_tyce.py tyc-sim2
   ```

3. Na webu: nastavit pacienta a cvik → **Mentor Start/Stop** → **Pacient Start/Stop**.
   Výsledek se zobrazí v grafu a uloží do historie.

## Jak to komunikuje

Tyč je HTTP klient, PC je server, data jsou JSON.

| Kdo  | Požadavek | Co dělá |
|------|-----------|---------|
| tyč  | `POST /api/registrace {"id"}` | ohlásí se, PC vrátí roli (první mentor, druhá pacient) |
| tyč  | `GET /api/prikaz?id=...` | 2× za sekundu se ptá na příkaz (`start` / `stop` / `nic`) |
| tyč  | `POST /api/data {"id","role","vzorky":[{"t","uhel"}]}` | po stopu pošle záznam |
| web  | `POST /api/start`, `/api/stop {"role"}` | spustí / zastaví záznam na tyči |
| web  | `POST /api/sezeni {"pacient","cvik"}` | nastaví sezení |
| web  | `POST /api/prohodit` | prohodí role tyčí |
| web  | `GET /api/stav`, `GET /api/historie` | data pro stránku |

Stavy tyče a co svítí: čekání (jedna modrá dioda), záznam (červená kulička
podle náklonu), odesílání (žlutá), hotovo (zelená), bez WiFi (tlumeně červená).

## Co je hotové a co ne

Hotové (základ):
- tyč: WiFi, registrace, čtení náklonu, kulička na pásku, záznam do paměti, odeslání
- PC: role, příkazy start/stop, uložení vzoru, porovnání **rozsahu** a **tempa**, historie v JSON
- web: přehled tyčí, ovládání, graf vzor vs. pokus, tabulka historie

TODO:
- průběžné odesílání dat z tyče (teď se posílá až celý záznam po stopu)
- kalibrace úhlu podle umístění desky v tyči
- porovnání **tvaru** (např. DTW), **plynulosti** a **opakovatelnosti**
- grafy vývoje v čase pro jednoho pacienta
- start/stop záznamu tlačítkem nebo gestem přímo na tyči
