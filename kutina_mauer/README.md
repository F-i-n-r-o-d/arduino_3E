# Rehabilitační tyč – hra pro dva hráče

Projekt: Kutina, Mauer

Vycházeli jsme z původního programu pro rehabilitační tyč (červená dioda se pohybuje podle náklonu a hráč ji musí
udržet ve žlutém pásu). Rozšířili jsme ho tak, aby proti sobě mohli hrát dva hráči, každý se svou tyčí. Tyče se přes
Wi-Fi připojí k serveru, který běží na počítači v Node.js. Server si hlídá skóre a pozice obou tyčí a posílá je na
webovou stránku, kde je celá hra vidět. Kdo první splní 10 úkolů, vyhrává.

Protože ve škole máme jen jednu tyč, přidali jsme i sólo hru a virtuální tyče v prohlížeči, abychom to mohli
testovat i bez hardwaru.

## Co je v projektu

- `tyc1/tyc1.ino` a `tyc2/tyc2.ino` – program pro tyče. Oba soubory jsou stejné, liší se jen v `HRAC_ID`.
- `server/server.js` – server (WebSocket), který spojuje tyče a web.
- `server/index.html` – webová stránka, kde je vidět hra (pozice, cíle, skóre).
- `server/tyc.html` – virtuální tyče v prohlížeči, na testování bez hardwaru.
- `server/simulace.js` – dvě automatické tyče, které hrají samy proti sobě (cvičný běh).

## Jak to spustit

### 1. Server

Je potřeba mít nainstalovaný [Node.js](https://nodejs.org). Pak v terminálu:

```
cd kutina_mauer/server
npm install
npm start
```

Server po spuštění vypíše IP adresu počítače (řádek `IP pro tyče: ... (Wi-Fi)`). Tu budete potřebovat do programu
pro tyč. Pokud se Windows zeptá na firewall, musí se přístup povolit, jinak se tyč k serveru nedostane.

### 2. Tyč

V souboru `tyc1/tyc1.ino` je nahoře část `NASTAVENÍ`. Tam je potřeba vyplnit:

- `ssid` a `pass` – Wi-Fi, na které je i počítač se serverem
- `SERVER_IP` – IP adresa, kterou vypsal server

Deska je Arduino Nano RP2040 Connect. V Library Manageru je potřeba mít knihovny NeoPixelConnect, Arduino_LSM6DSOX,
WiFiNINA a ArduinoHttpClient (tu poslední původní program nepoužíval, takže ji asi bude potřeba doinstalovat).

Po nahrání programu nechte tyč chvíli ležet normálně (LEDkami nahoru). Při zapnutí si totiž zapamatuje, jak vypadá
normální poloha, a podle toho pak pozná, kdy je otočená. Dokud se připojuje k Wi-Fi a serveru, svítí první dioda
modře. Co se děje, se dá sledovat i v Serial monitoru (115200).

### 3. Web

Otevřete v prohlížeči http://localhost:3000. Jakmile se tyč připojí, v konzoli serveru se objeví něco jako
`tyc1 připojena (..., Arduino)` a na stránce se začne hýbat kulička.

## Sólo hra

Když je připojená jen jedna tyč, na stránce se objeví tlačítko **Spustit sólo hru**. Druhá možnost je otočit tyč
a podržet ji tak 3 sekundy. Tlačítko se ukazuje jenom tehdy, když druhá tyč připojená není. Kdyby se druhá tyč
připojila během sólo hry, sólo skončí a začne normální hra dvou hráčů od nuly.

Jednu skutečnou tyč jde taky vyzkoušet proti virtuální tyči v prohlížeči: http://localhost:3000/tyc?id=tyc2

## Ovládání

- Náklon tyče hýbe červenou diodou. Když ji udržíte 1,5 s ve žlutém pásu, dostanete bod a pás se přesune jinam.
- Modrá dioda je soupeř (jeho žlutý pás nevidíte).
- Když tyč otočíte a podržíte 3 s, ukáže se skóre (svítí tolik bílých diod, kolik máte bodů). Po otočení zpět se
  hraje dál.
- Na konci hry bliká vítěz zeleně a poražený červeně. Novou hru spustíte otočením tyče na 3 s nebo tlačítkem
  **Nová hra** na webu.
- Když vypadne spojení, tyč hraje dál bez serveru a body pošle, až se zase připojí.

## Testování bez tyče

Spusťte server (`npm start`) a otevřete http://localhost:3000/tyc. Jsou tam dvě virtuální tyče, které se dají
naklánět myší nebo klávesami (tyč 1: A, D a W na otočení, tyč 2: šipky ← → a ↑ na otočení). Vedle si otevřete
http://localhost:3000 a uvidíte, jak se to propisuje.

Nebo `npm run simulace` – spustí server a dvě tyče, které hrají samy.

## Když něco nejde

- Tyč se nepřipojí k Wi-Fi: zkontrolujte `ssid` a `pass`. Deska umí jen 2,4 GHz Wi-Fi, na 5 GHz se nepřipojí.
- Tyč píše „Server nedostupny“: běží server? Sedí `SERVER_IP`? Je povolený firewall? Školní Wi-Fi někdy nedovolí,
  aby se zařízení viděla navzájem. V tom případě by měl pomoct hotspot z mobilu.
- Kompilace hlásí `stdio.h: No such file or directory`: to je kvůli diakritice ve jméně uživatele ve Windows (měli
  jsme to taky). Arduino pak nezvládne cestu ke svým souborům. Pomůže přesunout složku Arduina do cesty bez diakritiky.

## Jak spolu tyče a server komunikují

Posílají si krátké JSON zprávy přes WebSocket:

- tyč se přihlásí: `{"type":"join","room":"rehab","id":"tyc1"}`
- tyč posílá svoji pozici a kde má cíl: `{"type":"pos","pos":23,"cil":40}`
- tyč posílá skóre: `{"type":"score","score":3,"epoch":0}` (`epoch` je číslo hry, aby se body ze staré hry
  nezapočítaly do nové)
- tyč chce novou hru nebo sólo: `{"type":"rematch"}`, `{"type":"solo"}`
- server tyči posílá stav hry: `{"type":"status","state":"playing","solo":false,"won":null,"you":3,"opp":2,...}`
- server tyči posílá pozici soupeře: `{"type":"opp","pos":17}`

Web se připojuje stejně, jen jako divák (`"role":"viewer"`), a tlačítka na webu volají `POST /api/rooms/rehab/reset`
a `POST /api/rooms/rehab/solo`.
