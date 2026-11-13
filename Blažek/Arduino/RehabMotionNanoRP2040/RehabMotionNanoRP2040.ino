/*
  REHAB MOTION LAB
  Hardware: Arduino Nano RP2040 Connect
  Default IMU: built-in LSM6DSOX (accelerometer + gyroscope)
  Optional: external MPU6050, selected by USE_EXTERNAL_MPU6050 = 1
  LED strip: 64 x WS2812B / NeoPixel
  Network: WiFiNINA Access Point + HTTP web server

  Open http://192.168.4.1 after connecting to Wi-Fi:
    SSID: RehabMotion
    Password: Rehab2026

  Libraries (Arduino IDE Library Manager):
    - WiFiNINA
    - Arduino_LSM6DSOX
    - NeoPixelConnect (recommended for Nano RP2040 Connect)
    - Adafruit MPU6050 + Adafruit Unified Sensor (only if USE_EXTERNAL_MPU6050 = 1)

  Board package:
    Arduino Mbed OS Nano Boards
    Board: Arduino Nano RP2040 Connect

  SAFETY:
    - Power the 5V LED strip from a separate regulated 5V supply.
    - Connect the LED supply GND and Arduino GND together.
    - Do NOT power the 64 LEDs from the Nano board.
    - Use a 3.3V -> 5V logic-level shifter for the WS2812B data line.
    - Fit a 330-470 ohm series resistor at the data input and a 1000uF
      capacitor across the LED strip's 5V/GND input.
*/

#include <SPI.h>
#include <WiFiNINA.h>
#define USE_EXTERNAL_MPU6050 0

#if USE_EXTERNAL_MPU6050
  #include <Wire.h>
  #include <Adafruit_MPU6050.h>
  #include <Adafruit_Sensor.h>
  Adafruit_MPU6050 externalMPU;
  const char SENSOR_NAME[] = "MPU6050";
#else
  #include <Arduino_LSM6DSOX.h>
  const char SENSOR_NAME[] = "LSM6DSOX";
#endif

#include <NeoPixelConnect.h>
#include <math.h>
#include <string.h>

// -----------------------------------------------------------------------------
// USER CONFIGURATION
// -----------------------------------------------------------------------------

const char AP_SSID[] = "RehabMotion";
const char AP_PASSWORD[] = "Rehab2026"; // WPA2 password must be at least 8 chars
const uint8_t AP_CHANNEL = 6;

const uint8_t LED_GPIO = 18;            // Physical header D6 maps to RP2040 GPIO18
const uint16_t LED_COUNT = 64;

// Sensor orientation:
// This firmware measures roll around the sensor's X axis:
// accelerometer angle = atan2(Y, Z), gyroscope rate = gyro X.
// If your mechanical mounting uses the other axis, tell me how the board is
// placed and I can adapt the formula. If direction is reversed, set -1.0f.
const float SENSOR_SIGN = 1.0f;
const float FILTER_ALPHA = 0.985f;
const float RAD_TO_DEG_F = 57.2957795f;

// The box covers physical LEDs 28-36 (1-based positions).
// Left visible LEDs are 1-27; right visible LEDs are 37-64.
const int16_t LEFT_EDGE_INDEX = 26;    // 0-based LED 27
const int16_t RIGHT_EDGE_INDEX = 36;   // 0-based LED 37

int targetAngleDeg = 25;               // Editable by the web application: 5..60
uint32_t holdDurationMs = 3000;        // Editable by the web application: 500..15000

// -----------------------------------------------------------------------------
// GLOBAL OBJECTS AND STATE
// -----------------------------------------------------------------------------

WiFiServer server(80);
// NeoPixelConnect uses RP2040 GPIO numbers (not Arduino D-pin numbers).
NeoPixelConnect strip(LED_GPIO, LED_COUNT);

bool wifiReady = false;
bool sensorReady = false;
bool filterInitialized = false;
float neutralAngleDeg = 0.0f;
float filteredAngleDeg = 0.0f;
float rawAx = 0.0f, rawAy = 0.0f, rawAz = 1.0f;
float rawGx = 0.0f, rawGy = 0.0f, rawGz = 0.0f;
uint32_t lastImuMicros = 0;
uint32_t lastImuMillis = 0;
uint32_t lastLedUpdateMs = 0;
uint32_t lastWifiRetryMs = 0;
uint32_t sampleCounter = 0;

const char INDEX_HTML[] = R"HTMLPAGE(
<!DOCTYPE html>
<html lang="cs">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<meta name="theme-color" content="#101827">
<title>Rehab Motion Lab</title>
<style>
:root{--bg:#0b1220;--panel:#131e30;--panel2:#18263a;--line:#2a3a52;--text:#edf4ff;--muted:#9aabc2;--cyan:#54d7e7;--blue:#69a8ff;--green:#64d69b;--yellow:#ffc96b;--red:#ff7f86;--shadow:0 18px 45px rgba(0,0,0,.22)}
*{box-sizing:border-box}html{scroll-behavior:smooth}body{margin:0;background:radial-gradient(900px 500px at 90% -10%,#21395b 0%,transparent 65%),radial-gradient(700px 420px at -10% 30%,#17344a 0%,transparent 65%),var(--bg);color:var(--text);font-family:Inter,ui-sans-serif,system-ui,-apple-system,"Segoe UI",sans-serif;min-height:100vh}button,input{font:inherit}button{cursor:pointer}.app{max-width:1240px;margin:0 auto;padding:28px 22px 48px}.topbar{display:flex;align-items:center;justify-content:space-between;gap:20px;margin-bottom:26px}.brand{display:flex;align-items:center;gap:14px}.brand-mark{width:46px;height:46px;border-radius:15px;background:linear-gradient(145deg,#56dce5,#6d9eff);display:grid;place-items:center;color:#091522;font-size:24px;font-weight:900;box-shadow:0 8px 28px #44c6db38}.brand h1{font-size:20px;letter-spacing:.2px;margin:0}.brand p{margin:3px 0 0;color:var(--muted);font-size:12px;letter-spacing:.7px;text-transform:uppercase}.status{display:flex;align-items:center;gap:9px;padding:9px 13px;border:1px solid var(--line);background:#101d2c;border-radius:999px;color:var(--muted);font-size:13px}.dot{width:9px;height:9px;border-radius:50%;background:#8998aa;box-shadow:0 0 0 4px #8998aa1b}.status.online{color:#a7f1c7;border-color:#28664b}.status.online .dot{background:var(--green);box-shadow:0 0 0 4px #64d69b20}.status.offline .dot{background:var(--red);box-shadow:0 0 0 4px #ff7f8620}.modebar{display:flex;gap:8px;padding:6px;background:#0e1929;border:1px solid var(--line);border-radius:15px;width:max-content;margin-bottom:20px}.mode-btn{border:0;border-radius:10px;padding:11px 17px;background:transparent;color:var(--muted);font-weight:700}.mode-btn.active{background:linear-gradient(135deg,#27455c,#243c5f);color:#fff;box-shadow:inset 0 0 0 1px #47738b}.panel{background:linear-gradient(155deg,#17253a,#111c2d 75%);border:1px solid var(--line);border-radius:20px;box-shadow:var(--shadow)}.panel-pad{padding:22px}.section-head{display:flex;align-items:flex-start;justify-content:space-between;gap:16px;margin-bottom:17px}.section-head h2,.section-head h3{margin:0;font-size:19px}.section-head p{margin:5px 0 0;color:var(--muted);font-size:13px;line-height:1.5}.eyebrow{font-size:10px;letter-spacing:1.5px;text-transform:uppercase;color:var(--cyan);font-weight:800;margin-bottom:7px}.dev-grid{display:grid;grid-template-columns:1.4fr .8fr;gap:16px}.metrics{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;margin-bottom:16px}.metric{padding:17px;border-radius:15px;background:#101b2b;border:1px solid #263850;min-width:0}.metric .label{color:var(--muted);font-size:12px}.metric .value{font-size:29px;font-weight:800;letter-spacing:-1px;margin-top:5px;font-variant-numeric:tabular-nums}.metric .unit{font-size:13px;color:var(--muted);font-weight:600;letter-spacing:0}.metric .hint{color:var(--muted);font-size:11px;margin-top:4px}.led-panel{margin-top:16px}.led-scroll{overflow-x:auto;padding:9px 3px 6px}.led-track{display:grid;grid-template-columns:repeat(64,minmax(8px,1fr));gap:3px;min-width:690px;align-items:center}.led{height:19px;border-radius:6px;background:#28364a;border:1px solid #354862;transition:background .15s,box-shadow .15s}.led.on-right{background:linear-gradient(180deg,#8ce5ff,#2698e8);box-shadow:0 0 12px #39b7ff88;border-color:#81dcff}.led.on-left{background:linear-gradient(180deg,#a2efc2,#33b977);box-shadow:0 0 12px #33c98488;border-color:#9af0bd}.led.boxled{background:#efb94a;border-color:#ffe09a;box-shadow:0 0 8px #efb94a44;height:24px}.led-labels{display:grid;grid-template-columns:27fr 9fr 28fr;gap:4px;min-width:690px;margin-top:8px;text-align:center;color:var(--muted);font-size:10px;letter-spacing:.7px;text-transform:uppercase}.led-labels span:nth-child(2){color:var(--yellow)}.legend{display:flex;gap:15px;flex-wrap:wrap;margin-top:15px;color:var(--muted);font-size:11px}.legend span{display:flex;align-items:center;gap:6px}.swatch{width:9px;height:9px;border-radius:3px;display:inline-block}.controls{display:grid;gap:15px}.control{padding:15px;border:1px solid #293c55;background:#101b2b;border-radius:15px}.control label{display:flex;justify-content:space-between;align-items:center;gap:12px;font-weight:700;font-size:13px;margin-bottom:11px}.control label small{color:var(--muted);font-weight:500}.range{width:100%;accent-color:var(--cyan);cursor:pointer}.range-ends{display:flex;justify-content:space-between;color:var(--muted);font-size:11px;margin-top:5px}.inline-row{display:flex;gap:9px;align-items:center;flex-wrap:wrap}.pill{padding:7px 10px;border-radius:999px;background:#1e3048;border:1px solid #344b67;color:#c7d9ed;font-size:11px}.primary,.secondary,.ghost{border:0;border-radius:11px;padding:11px 14px;font-weight:750;transition:transform .15s,filter .15s}.primary{background:linear-gradient(135deg,#42c6d8,#568df0);color:#081725}.secondary{background:#26415d;color:#e5f4ff;border:1px solid #3a5b7d}.ghost{background:#1a293d;color:#c8d7e8;border:1px solid #33465d}.primary:hover,.secondary:hover,.ghost:hover{filter:brightness(1.12);transform:translateY(-1px)}button:disabled{opacity:.45;cursor:not-allowed;transform:none}.toggle-line{display:flex;align-items:center;gap:9px;color:var(--muted);font-size:12px}.toggle-line input{accent-color:var(--cyan);width:17px;height:17px}.small-note{font-size:11px;color:var(--muted);line-height:1.5}.raw{background:#08111e;border:1px solid #27384e;border-radius:13px;padding:13px;min-height:132px;max-height:220px;overflow:auto;color:#a9d9c6;font:11px/1.55 ui-monospace,SFMono-Regular,Consolas,monospace;white-space:pre-wrap;word-break:break-word}.progress-line{height:10px;border-radius:99px;background:#26364c;overflow:hidden;margin-top:11px}.progress-fill{height:100%;width:0;background:linear-gradient(90deg,#52d8a0,#59cce9,#699cff);border-radius:99px;transition:width .2s}.progress-caption{display:flex;justify-content:space-between;margin-top:7px;font-size:11px;color:var(--muted)}.hide{display:none!important}.patient-wrap{display:grid;grid-template-columns:1fr .8fr;gap:16px}.game-card{min-height:480px;display:flex;flex-direction:column;align-items:center;justify-content:center;text-align:center;position:relative;overflow:hidden;background:radial-gradient(ellipse at 50% 82%,#23513e 0%,#1c3a3d 25%,transparent 60%),linear-gradient(150deg,#172b42,#101b2c);border:1px solid #35536a;border-radius:20px;padding:25px}.game-card:before,.game-card:after{content:"";position:absolute;border-radius:50%;border:1px solid #83d9cd18;width:360px;height:360px;pointer-events:none}.game-card:after{width:470px;height:470px}.game-status{z-index:1;color:#b4c8d9;font-size:12px;text-transform:uppercase;letter-spacing:1.6px;font-weight:800}.flower-stage{height:220px;width:100%;display:flex;align-items:flex-end;justify-content:center;position:relative;z-index:1}.pot{font-size:76px;line-height:1;filter:drop-shadow(0 12px 13px #0005);z-index:2}.plant{position:absolute;bottom:67px;font-size:78px;transform-origin:bottom center;transition:transform .4s ease,filter .4s}.plant.growing{transform:scale(1.35);filter:drop-shadow(0 0 20px #ffcf6e88)}.flower-count{margin-top:10px;color:#b8d1db;font-size:13px;z-index:1}.game-title{font-size:27px;margin:16px 0 6px;z-index:1}.game-sub{color:#b7c8d7;font-size:14px;max-width:430px;line-height:1.55;z-index:1}.game-target{display:flex;align-items:center;justify-content:center;gap:9px;margin:18px 0 10px;z-index:1;font-size:13px;color:#c6d8e7}.target-num{font-size:31px;font-weight:900;color:#fff;font-variant-numeric:tabular-nums}.hold-track{width:min(340px,90%);height:12px;background:#34485b;border-radius:99px;overflow:hidden;margin:8px auto 7px;z-index:1}.hold-fill{height:100%;width:0;background:linear-gradient(90deg,#71dda6,#ffdf83);transition:width .1s}.hold-label{font-size:12px;color:#c6d8e7;z-index:1}.game-actions{display:flex;gap:9px;margin-top:18px;z-index:1}.side-pill{padding:8px 13px;border:1px solid #3e6071;background:#19344a;border-radius:999px;color:#a8eddf;font-size:12px;font-weight:800}.game-side-panel{display:flex;flex-direction:column;gap:13px}.goal-card{padding:20px;border-radius:16px;background:#101b2b;border:1px solid #2b4057}.goal-card h3{margin:0 0 6px;font-size:16px}.goal-card p{margin:0;color:var(--muted);font-size:12px;line-height:1.5}.goal-big{font-size:39px;font-weight:900;letter-spacing:-1.5px;margin:12px 0 2px}.goal-label{font-size:11px;color:var(--muted)}.side-direction{display:flex;gap:9px;margin-top:14px}.side-direction div{flex:1;border-radius:12px;padding:13px 10px;background:#1a2a3e;border:1px solid #30455e;text-align:center}.side-direction strong{display:block;font-size:18px}.side-direction span{font-size:11px;color:var(--muted)}.connection-box{padding:15px;border:1px solid #2b4057;background:#101b2b;border-radius:14px}.connection-box strong{display:block;font-size:13px;margin-bottom:5px}.connection-box p{margin:0;color:var(--muted);font-size:11px;line-height:1.5}.footer{margin-top:18px;color:#7f91a8;font-size:11px;line-height:1.6;text-align:center}.field-row{display:grid;grid-template-columns:1fr 1fr;gap:9px}.number-input{width:100%;background:#0c1726;border:1px solid #344b67;border-radius:9px;padding:10px;color:#fff;outline:none}.number-input:focus{border-color:var(--cyan)}.notice{padding:12px 14px;border:1px solid #725a31;background:#372d1b;color:#f1d69c;border-radius:12px;font-size:12px;line-height:1.5;margin-top:14px}
@media(max-width:850px){.dev-grid,.patient-wrap{grid-template-columns:1fr}.game-side-panel{display:grid;grid-template-columns:1fr 1fr}.game-card{min-height:440px}}
@media(max-width:560px){.app{padding:18px 12px 32px}.topbar{align-items:flex-start;flex-direction:column}.status{align-self:flex-start}.modebar{width:100%}.mode-btn{flex:1;padding:10px 7px;font-size:12px}.metrics{grid-template-columns:1fr 1fr}.metric:last-child{grid-column:span 2}.metric .value{font-size:25px}.panel-pad{padding:16px}.section-head{flex-direction:column}.game-side-panel{grid-template-columns:1fr}.game-title{font-size:23px}.game-card{padding:18px 12px}.flower-stage{height:190px}}
</style>
</head>
<body>
<div class="app">
  <header class="topbar">
    <div class="brand"><div class="brand-mark">✦</div><div><h1>Rehab Motion Lab</h1><p>Motion therapy · Nano RP2040 Connect</p></div></div>
    <div class="status offline" id="connection"><span class="dot"></span><span id="connectionText">Připojování k Arduinu…</span></div>
  </header>

  <nav class="modebar" aria-label="Režim aplikace">
    <button class="mode-btn active" data-mode="debug">⌘ Vývojář</button>
    <button class="mode-btn" data-mode="patient">✿ Pacient</button>
  </nav>

  <main>
    <section id="debugMode">
      <div class="metrics">
        <div class="metric"><div class="label">Aktuální úhel</div><div class="value" id="angleValue">0.0<span class="unit"> °</span></div><div class="hint" id="directionValue">Středová poloha</div></div>
        <div class="metric"><div class="label">Progres k cíli</div><div class="value" id="progressValue">0<span class="unit"> %</span></div><div class="progress-line"><div class="progress-fill" id="progressFill"></div></div><div class="progress-caption"><span>0 %</span><span id="targetCaption">Cíl 25°</span></div></div>
        <div class="metric"><div class="label">Rozsah LED</div><div class="value" id="ledCountValue">0<span class="unit"> / 28</span></div><div class="hint">Aktivní diody od středu</div></div>
      </div>

      <div class="dev-grid">
        <div class="panel panel-pad">
          <div class="section-head"><div><div class="eyebrow">Hardware monitor</div><h2>LED lišta · 64 pozic</h2><p>Středová krabička zakrývá pozice 28–36. Aktivace postupuje od krabičky směrem ven.</p></div><span class="pill" id="sensorPill">Senzor: čekám</span></div>
          <div class="led-scroll"><div class="led-track" id="ledTrack" aria-label="Zobrazení 64 LED"></div><div class="led-labels"><span>Levá strana · 1–27</span><span>BOX · 28–36</span><span>Pravá strana · 37–64</span></div></div>
          <div class="legend"><span><i class="swatch" style="background:#46c987"></i>Rotace doleva</span><span><i class="swatch" style="background:#4ab7f5"></i>Rotace doprava</span><span><i class="swatch" style="background:#efbd55"></i>Středová krabička</span></div>
          <div class="notice">Fyzický směr LED závisí na tom, jak je pásek zapojený. Pokud se při pohybu doprava rozsvěcí opačná část, lze změnit směr v konstantách firmware.</div>
        </div>

        <div class="panel panel-pad">
          <div class="section-head"><div><div class="eyebrow">Ovládání a kalibrace</div><h2>Test senzoru</h2><p>Simulaci zapni pro test webového rozhraní bez pohybu zařízení.</p></div></div>
          <div class="controls">
            <div class="control">
              <label for="simToggle">Simulační režim <small id="sourceLabel">Živá data</small></label>
              <div class="toggle-line"><input type="checkbox" id="simToggle"><span>Zapnout simulaci posuvníkem a šipkami</span></div>
              <input class="range" type="range" id="simSlider" min="-60" max="60" value="0" step="1" disabled>
              <div class="range-ends"><span>−60° vlevo</span><span id="simValue">0°</span><span>+60° vpravo</span></div>
              <div class="inline-row" style="margin-top:11px"><button class="ghost" id="simMinus" disabled>← −5°</button><button class="ghost" id="simPlus" disabled>+5° →</button><button class="ghost" id="simReset" disabled>Reset</button></div>
            </div>
            <div class="control">
              <label>Nastavení cvičení</label>
              <div class="field-row"><div><div class="small-note" style="margin-bottom:6px">Cílový úhel (°)</div><input class="number-input" type="number" id="targetInput" min="5" max="60" value="25"></div><div><div class="small-note" style="margin-bottom:6px">Výdrž (sekundy)</div><input class="number-input" type="number" id="holdInput" min="1" max="15" value="3"></div></div>
              <button class="secondary" id="applyConfig" style="width:100%;margin-top:11px">Uložit nastavení do Arduina</button>
            </div>
            <div class="inline-row"><button class="primary" id="calibrateBtn">◎ Kalibrovat nulovou polohu</button><span class="small-note" id="calibrateInfo">Při kalibraci drž zařízení v neutrální poloze.</span></div>
            <div><div class="small-note" style="margin-bottom:7px">Surová data senzoru (akcelerometr v g, gyroskop ve °/s)</div><pre class="raw" id="rawData">Čekám na první data z Arduina…</pre></div>
          </div>
        </div>
      </div>
    </section>

    <section id="patientMode" class="hide">
      <div class="patient-wrap">
        <div class="game-card">
          <div class="game-status" id="gameStatus">Připraveno k cvičení</div>
          <div class="flower-stage"><div class="plant" id="plant">🌱</div><div class="pot">🪴</div></div>
          <div class="flower-count" id="flowerCount">Rozkvetlé květiny: 0</div>
          <h2 class="game-title" id="gameTitle">Pomoz květině vyrůst!</h2>
          <p class="game-sub" id="gameInstruction">Stiskni Start. Potom pomalu nakloň zařízení na zobrazenou stranu a drž cílový úhel.</p>
          <div class="game-target"><span id="sideEmoji">➡️</span><span id="sideText">Pravá strana</span><span class="target-num" id="gameTarget">25°</span></div>
          <div class="hold-track"><div class="hold-fill" id="holdFill"></div></div>
          <div class="hold-label" id="holdLabel">Výdrž: 0.0 / 3.0 s</div>
          <div class="game-actions"><button class="primary" id="gameStart">▶ Spustit cvičení</button><button class="ghost" id="gameReset">↻ Začít znovu</button></div>
        </div>
        <div class="game-side-panel">
          <div class="goal-card"><div class="eyebrow">Aktuální měření</div><h3>Tvůj pohyb</h3><div class="goal-big" id="patientAngle">0.0°</div><div class="goal-label" id="patientDirection">Zatím bez náklonu</div><div class="progress-line"><div class="progress-fill" id="patientProgress"></div></div><div class="progress-caption"><span>0 %</span><span id="patientProgressText">0 % k cíli</span></div></div>
          <div class="goal-card"><div class="eyebrow">Cvičební plán</div><h3>Obě strany těla</h3><p>Po úspěšném udržení úhlu na jedné straně se automaticky vystřídá druhá strana.</p><div class="side-direction"><div><strong>⬅️</strong><span>Levá strana</span></div><div><strong>➡️</strong><span>Pravá strana</span></div></div><div class="goal-big" id="bloomTotal">0</div><div class="goal-label">Počet zalití / splněných výdrží</div></div>
          <div class="connection-box"><strong id="patientConnectionTitle">Čekám na připojení</strong><p id="patientConnectionText">Připoj se k Wi-Fi síti RehabMotion a otevři tuto stránku na adrese 192.168.4.1.</p></div>
        </div>
      </div>
    </section>
  </main>
  <div class="footer">Rehab Motion Lab · Prototyp pro výukový a rehabilitační projekt · Nejedná se o certifikovaný zdravotnický prostředek.</div>
</div>
<script>
(() => {
  'use strict';
  const API_BASE = (location.protocol === 'file:' || !location.hostname) ? 'http://192.168.4.1' : '';
  const $ = (id) => document.getElementById(id);
  const els = {
    connection:$('connection'), connectionText:$('connectionText'), angle:$('angleValue'), direction:$('directionValue'), progress:$('progressValue'), progressFill:$('progressFill'), targetCaption:$('targetCaption'), ledCount:$('ledCountValue'), ledTrack:$('ledTrack'), sensorPill:$('sensorPill'), raw:$('rawData'), simToggle:$('simToggle'), simSlider:$('simSlider'), simValue:$('simValue'), sourceLabel:$('sourceLabel'), targetInput:$('targetInput'), holdInput:$('holdInput'), patientAngle:$('patientAngle'), patientDirection:$('patientDirection'), patientProgress:$('patientProgress'), patientProgressText:$('patientProgressText'), plant:$('plant'), flowerCount:$('flowerCount'), gameStatus:$('gameStatus'), gameTitle:$('gameTitle'), gameInstruction:$('gameInstruction'), sideEmoji:$('sideEmoji'), sideText:$('sideText'), gameTarget:$('gameTarget'), holdFill:$('holdFill'), holdLabel:$('holdLabel'), gameStart:$('gameStart'), patientConnectionTitle:$('patientConnectionTitle'), patientConnectionText:$('patientConnectionText'), bloomTotal:$('bloomTotal')
  };
  const ledNodes = [];
  for (let i = 1; i <= 64; i++) {
    const node = document.createElement('div'); node.className = 'led' + (i >= 28 && i <= 36 ? ' boxled' : ''); node.title = 'LED ' + i;
    els.ledTrack.appendChild(node); ledNodes.push(node);
  }
  let liveData = null, connected = false, simulation = false, simAngle = 0, targetAngle = 25, holdMs = 3000, pollBusy = false;
  let lastAngle = 0;
  const game = { running:false, side:1, holdStart:0, completed:0, switchAt:0, messageUntil:0 };

  function apiUrl(path) { return API_BASE + path; }
  async function apiGet(path, timeoutMs=1800) {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), timeoutMs);
    try {
      const response = await fetch(apiUrl(path), {method:'GET', cache:'no-store', signal:controller.signal});
      if (!response.ok) throw new Error('HTTP ' + response.status);
      return await response.json();
    } finally { clearTimeout(timeout); }
  }
  function setConnection(networkOk, message, sensorName, sensorOk) {
    connected = networkOk;
    els.connection.className = 'status ' + (networkOk ? 'online' : 'offline');
    els.connectionText.textContent = message;
    const sensorAvailable = (sensorOk === undefined) ? networkOk : sensorOk;
    els.sensorPill.textContent = sensorAvailable ? 'Senzor: ' + (sensorName || 'IMU') + ' OK' : 'Senzor: chyba / offline';
    els.patientConnectionTitle.textContent = networkOk ? 'Zařízení je připojeno' : 'Zařízení není připojeno';
    els.patientConnectionText.textContent = networkOk ? 'Úhel se měří přímo na Arduinu. Pohybuj se pomalu a plynule.' : 'Připoj počítač k Wi-Fi síti RehabMotion a otevři http://192.168.4.1.';
  }
  function progressFor(angle) { return Math.min(100, Math.abs(angle) / Math.max(1,targetAngle) * 100); }
  function renderStrip(angle, progress) {
    const sideCount = angle >= 0 ? 28 : 27;
    const count = Math.round(progress / 100 * sideCount);
    for (let i = 1; i <= 64; i++) {
      const node = ledNodes[i-1];
      node.classList.remove('on-left','on-right');
      if (i >= 28 && i <= 36) continue;
      const active = angle >= 0 ? (i >= 37 && i <= 36 + count) : (i <= 27 && i >= 28 - count);
      if (active) node.classList.add(angle >= 0 ? 'on-right' : 'on-left');
    }
  }
  function currentData() {
    if (simulation) return {angle:simAngle, progress:progressFor(simAngle), sensorOk:true, raw:{mode:'SIMULACE', angle:simAngle, note:'Simulovaná data – nejsou z fyzického senzoru.'}, ledCount:Math.round(progressFor(simAngle)/100*(simAngle>=0?28:27))};
    return liveData || {angle:0, progress:0, sensorOk:false, raw:{status:'Čekám na Arduino…'}, ledCount:0};
  }
  function draw() {
    const data = currentData();
    const angle = Number(data.angle || 0), progress = progressFor(angle);
    lastAngle = angle;
    const signText = Math.abs(angle) < 0.6 ? 'Středová poloha' : (angle < 0 ? 'Rotace doleva' : 'Rotace doprava');
    els.angle.innerHTML = angle.toFixed(1) + '<span class="unit"> °</span>';
    els.direction.textContent = signText;
    els.progress.innerHTML = progress.toFixed(0) + '<span class="unit"> %</span>';
    els.progressFill.style.width = progress + '%';
    els.targetCaption.textContent = 'Cíl ' + targetAngle + '°';
    const maxLeds = angle >= 0 ? 28 : 27;
    els.ledCount.innerHTML = Math.round(progress/100*maxLeds) + '<span class="unit"> / ' + maxLeds + '</span>';
    renderStrip(angle, progress);
    els.patientAngle.textContent = angle.toFixed(1) + '°';
    els.patientDirection.textContent = signText;
    els.patientProgress.style.width = progress + '%';
    els.patientProgressText.textContent = progress.toFixed(0) + ' % k cíli';
    els.simValue.textContent = simAngle.toFixed(0) + '°';
    els.sourceLabel.textContent = simulation ? 'Simulace zapnuta' : (connected ? 'Živá data z Arduina' : 'Čeká na Arduino');
    if (simulation) els.raw.textContent = JSON.stringify(data.raw,null,2);
    else if (liveData) els.raw.textContent = JSON.stringify(liveData.raw || liveData,null,2);
    else els.raw.textContent = 'Čekám na první data z Arduina…';
  }
  async function poll() {
    if (pollBusy) return;
    pollBusy = true;
    try {
      const d = await apiGet('/api/data');
      if (!d || d.ok !== true) throw new Error('Neplatná odpověď');
      liveData = d; setConnection(true, d.sensorOk ? 'Připojeno · ' + (d.ip || 'Arduino') : 'Wi-Fi připojena · chyba senzoru', d.sensor || 'IMU', !!d.sensorOk);
      if (Number.isFinite(Number(d.targetAngle))) { targetAngle = Number(d.targetAngle); if (document.activeElement !== els.targetInput) els.targetInput.value = targetAngle; }
      if (Number.isFinite(Number(d.holdMs))) { holdMs = Number(d.holdMs); if (document.activeElement !== els.holdInput) els.holdInput.value = (holdMs/1000).toFixed(1).replace(/\.0$/,''); }
    } catch (e) { setConnection(false, 'Arduino nedostupné'); }
    finally { pollBusy = false; }
    draw(); updateGame();
  }
  function updateSimulationControls() {
    simulation = els.simToggle.checked;
    els.simSlider.disabled = !simulation; $('simMinus').disabled = !simulation; $('simPlus').disabled = !simulation; $('simReset').disabled = !simulation;
    if (simulation) els.simSlider.value = simAngle;
    draw();
  }
  function setSim(v) { simAngle = Math.max(-60,Math.min(60,Number(v)||0)); els.simSlider.value = simAngle; draw(); updateGame(); }
  els.simToggle.addEventListener('change', updateSimulationControls);
  els.simSlider.addEventListener('input', e => setSim(e.target.value));
  $('simMinus').addEventListener('click', () => setSim(simAngle-5));
  $('simPlus').addEventListener('click', () => setSim(simAngle+5));
  $('simReset').addEventListener('click', () => setSim(0));
  window.addEventListener('keydown', e => {
    if (!simulation || ['INPUT','TEXTAREA'].includes(document.activeElement.tagName)) return;
    const step = e.shiftKey ? 5 : 1;
    if (e.key === 'ArrowLeft' || e.key === 'ArrowDown') {e.preventDefault();setSim(simAngle-step);}
    if (e.key === 'ArrowRight' || e.key === 'ArrowUp') {e.preventDefault();setSim(simAngle+step);}
  });
  document.querySelectorAll('.mode-btn').forEach(btn => btn.addEventListener('click', () => {
    document.querySelectorAll('.mode-btn').forEach(b=>b.classList.toggle('active',b===btn));
    $('debugMode').classList.toggle('hide',btn.dataset.mode!=='debug');
    $('patientMode').classList.toggle('hide',btn.dataset.mode!=='patient');
  }));
  $('applyConfig').addEventListener('click', async () => {
    const t = Math.max(5,Math.min(60,Number(els.targetInput.value)||25));
    const h = Math.max(1,Math.min(15,Number(els.holdInput.value)||3));
    targetAngle=t; holdMs=h*1000; els.targetInput.value=t; els.holdInput.value=h; draw();
    try { await apiGet('/api/config?target='+encodeURIComponent(t)+'&hold='+encodeURIComponent(holdMs)); els.calibrateInfo.textContent='Nastavení bylo uloženo do Arduina.'; }
    catch(e) { els.calibrateInfo.textContent='Arduino není dostupné. Nastavení platí jen v této stránce.'; }
  });
  $('calibrateBtn').addEventListener('click', async () => {
    els.calibrateInfo.textContent='Kalibrace… nehýbej zařízením.';
    try { const r=await apiGet('/api/calibrate',4000); els.calibrateInfo.textContent=r.message || 'Nulová poloha nastavena.'; }
    catch(e) { els.calibrateInfo.textContent='Kalibrace se nezdařila – zkontroluj spojení.'; }
  });
  function setGameSide(side) {
    game.side=side; game.holdStart=0;
    els.sideEmoji.textContent=side===1?'➡️':'⬅️';
    els.sideText.textContent=side===1?'Pravá strana':'Levá strana';
    els.gameTarget.textContent=targetAngle+'°';
    els.holdFill.style.width='0%';
    els.holdLabel.textContent='Výdrž: 0.0 / '+(holdMs/1000).toFixed(1)+' s';
  }
  function updateGame() {
    if (!game.running) return;
    const now=Date.now();
    if (game.switchAt && now < game.switchAt) return;
    if (game.switchAt && now >= game.switchAt) { game.switchAt=0; els.gameStatus.textContent='Pokračuj na druhou stranu'; els.gameTitle.textContent='Teď druhá strana!'; }
    const angle=Number(currentData().angle)||0;
    const reached=game.side===1 ? angle>=targetAngle : angle<=-targetAngle;
    if (reached) {
      if (!game.holdStart) {game.holdStart=now;els.gameStatus.textContent='Výborně! Drž polohu…';}
      const elapsed=now-game.holdStart, pct=Math.min(100,elapsed/holdMs*100), seconds=Math.min(holdMs,elapsed)/1000;
      els.holdFill.style.width=pct+'%';
      els.holdLabel.textContent='Výdrž: '+seconds.toFixed(1)+' / '+(holdMs/1000).toFixed(1)+' s';
      if (elapsed>=holdMs) {
        game.completed++; els.bloomTotal.textContent=game.completed; els.flowerCount.textContent='Rozkvetlé květiny: '+game.completed;
        els.plant.textContent='🌸'; els.plant.classList.add('growing');
        els.gameStatus.textContent='Skvělé! Cíl splněn'; els.gameTitle.textContent='Květina rozkvetla!';
        els.gameInstruction.textContent='Výborná práce. Za chvíli pokračuj na opačnou stranu.';
        game.holdStart=0; game.switchAt=now+1700;
        setTimeout(()=>{ if(game.running){els.plant.textContent='🌱';els.plant.classList.remove('growing');setGameSide(game.side*-1);els.gameInstruction.textContent='Pomalu dosáhni cílového úhlu a chvíli jej udrž.';} },1500);
      }
    } else {
      game.holdStart=0; els.holdFill.style.width='0%';
      els.holdLabel.textContent='Výdrž: 0.0 / '+(holdMs/1000).toFixed(1)+' s';
      els.gameStatus.textContent='Pomalu se přibliž k cíli';
    }
  }
  els.gameStart.addEventListener('click', () => {
    game.running=!game.running;
    els.gameStart.textContent=game.running?'Ⅱ Pozastavit':'▶ Pokračovat';
    els.gameStatus.textContent=game.running?'Cvičení spuštěno':'Cvičení pozastaveno';
    if (game.running) updateGame();
  });
  $('gameReset').addEventListener('click', () => {
    game.running=false;game.side=1;game.holdStart=0;game.completed=0;game.switchAt=0;
    els.gameStart.textContent='▶ Spustit cvičení';els.bloomTotal.textContent='0';els.flowerCount.textContent='Rozkvetlé květiny: 0';els.plant.textContent='🌱';els.plant.classList.remove('growing');els.gameTitle.textContent='Pomoz květině vyrůst!';els.gameInstruction.textContent='Stiskni Start. Potom pomalu nakloň zařízení na zobrazenou stranu a drž cílový úhel.';els.gameStatus.textContent='Připraveno k cvičení';setGameSide(1);
  });
  setGameSide(1);
  updateSimulationControls();
  poll();
  setInterval(poll,250);
})();
</script>
</body>
</html>
)HTMLPAGE";

// -----------------------------------------------------------------------------
// SMALL HELPERS
// -----------------------------------------------------------------------------

float wrap180(float angle) {
  while (angle > 180.0f) angle -= 360.0f;
  while (angle < -180.0f) angle += 360.0f;
  return angle;
}

float currentProgress() {
  float p = fabsf(filteredAngleDeg) / (float)targetAngleDeg * 100.0f;
  if (p > 100.0f) p = 100.0f;
  if (p < 0.0f) p = 0.0f;
  return p;
}

bool beginMotionSensor() {
#if USE_EXTERNAL_MPU6050
  if (!externalMPU.begin(0x68, &Wire)) return false;
  externalMPU.setAccelerometerRange(MPU6050_RANGE_4_G);
  externalMPU.setGyroRange(MPU6050_RANGE_500_DEG);
  externalMPU.setFilterBandwidth(MPU6050_BAND_21_HZ);
  return true;
#else
  return IMU.begin();
#endif
}

bool readMotionSample(float &ax, float &ay, float &az,
                      float &gx, float &gy, float &gz) {
#if USE_EXTERNAL_MPU6050
  static uint32_t lastExternalReadMs = 0;
  if (millis() - lastExternalReadMs < 8UL) return false;
  lastExternalReadMs = millis();
  sensors_event_t accelEvent, gyroEvent, tempEvent;
  externalMPU.getEvent(&accelEvent, &gyroEvent, &tempEvent);
  // Adafruit MPU6050 reports acceleration in m/s^2 and gyro in rad/s.
  ax = accelEvent.acceleration.x / 9.80665f;
  ay = accelEvent.acceleration.y / 9.80665f;
  az = accelEvent.acceleration.z / 9.80665f;
  gx = gyroEvent.gyro.x * RAD_TO_DEG_F;
  gy = gyroEvent.gyro.y * RAD_TO_DEG_F;
  gz = gyroEvent.gyro.z * RAD_TO_DEG_F;
  return true;
#else
  if (!IMU.accelerationAvailable() || !IMU.gyroscopeAvailable()) return false;
  if (!IMU.readAcceleration(ax, ay, az)) return false;
  if (!IMU.readGyroscope(gx, gy, gz)) return false;
  return true;
#endif
}

void calibrateNeutral() {
  // Keep the bar still and in its normal resting position during calibration.
  const uint8_t wantedSamples = 45;
  uint8_t collected = 0;
  float angleSum = 0.0f;
  uint32_t started = millis();

  while (collected < wantedSamples && (millis() - started) < 2500UL) {
    float ax, ay, az, gx, gy, gz;
    if (readMotionSample(ax, ay, az, gx, gy, gz)) {
      float a = atan2f(ay, az) * RAD_TO_DEG_F;
      angleSum += a;
      collected++;
    }
    delay(3);
  }

  if (collected > 0) {
    neutralAngleDeg = angleSum / (float)collected;
    filteredAngleDeg = 0.0f;
    filterInitialized = true;
    lastImuMicros = micros();
    Serial.print(F("Neutral angle calibrated: "));
    Serial.print(neutralAngleDeg, 2);
    Serial.print(F(" deg using "));
    Serial.println(collected);
  } else {
    Serial.println(F("Calibration failed: no accelerometer samples."));
  }
}

void updateSensor() {
  if (!sensorReady) return;

  {
    float ax, ay, az;
    float gx, gy, gz;
    if (!readMotionSample(ax, ay, az, gx, gy, gz)) return;

    rawAx = ax;
    rawAy = ay;
    rawAz = az;
    rawGx = gx;
    rawGy = gy;
    rawGz = gz;

    const uint32_t nowUs = micros();
    float dt = (nowUs - lastImuMicros) / 1000000.0f;
    lastImuMicros = nowUs;

    // Ignore implausibly large time gaps rather than integrating a huge jump.
    if (dt < 0.001f || dt > 0.08f) dt = 0.01f;

    const float accelAngle = wrap180(
      atan2f(rawAy, rawAz) * RAD_TO_DEG_F - neutralAngleDeg
    ) * SENSOR_SIGN;
    const float gyroRate = rawGx * SENSOR_SIGN;

    if (!filterInitialized) {
      filteredAngleDeg = accelAngle;
      filterInitialized = true;
    } else {
      const float gyroAngle = filteredAngleDeg + gyroRate * dt;
      filteredAngleDeg = FILTER_ALPHA * gyroAngle + (1.0f - FILTER_ALPHA) * accelAngle;
    }

    // This device is intended for gentle range-of-motion exercises.
    // Keep display/output bounded; this is not a clinical safety limiter.
    if (filteredAngleDeg > 90.0f) filteredAngleDeg = 90.0f;
    if (filteredAngleDeg < -90.0f) filteredAngleDeg = -90.0f;

    lastImuMillis = millis();
    sampleCounter++;
  }
}

void updateLedStrip() {
  if (millis() - lastLedUpdateMs < 35UL) return;
  lastLedUpdateMs = millis();

  strip.neoPixelClear(false);

  const float progress = currentProgress();
  const bool rightSide = (filteredAngleDeg >= 0.0f);
  const uint8_t maxCount = rightSide ? 28 : 27;
  const uint8_t litCount = (uint8_t)lroundf((progress / 100.0f) * maxCount);

  // Dim amber LEDs represent the covered middle enclosure (28-36).
  for (int i = 27; i <= 35; i++) {
    strip.neoPixelSetValue(i, 7, 4, 0, false);
  }

  // Positive angle lights from LED 37 outward to LED 64.
  // Negative angle lights from LED 27 inward to LED 1.
  for (uint8_t i = 0; i < litCount; i++) {
    int16_t index = rightSide ? (RIGHT_EDGE_INDEX + i) : (LEFT_EDGE_INDEX - i);
    if (index < 0 || index >= (int16_t)LED_COUNT) continue;

    if (rightSide) {
      strip.neoPixelSetValue(index, 0, 32, 68, false);
    } else {
      strip.neoPixelSetValue(index, 0, 58, 24, false);
    }
  }

  strip.neoPixelShow();
}

// -----------------------------------------------------------------------------
// HTTP SERVER
// -----------------------------------------------------------------------------

void sendHeaders(WiFiClient &client, const char *status, const char *contentType, size_t contentLength) {
  client.print(F("HTTP/1.1 "));
  client.println(status);
  client.print(F("Content-Type: "));
  client.println(contentType);
  client.print(F("Content-Length: "));
  client.println((unsigned long)contentLength);
  client.println(F("Cache-Control: no-store, no-cache, must-revalidate"));
  client.println(F("Access-Control-Allow-Origin: *"));
  client.println(F("Access-Control-Allow-Methods: GET, OPTIONS"));
  client.println(F("Access-Control-Allow-Headers: Content-Type"));
  client.println(F("Connection: close"));
  client.println();
}

void sendJson(WiFiClient &client, const char *status, const char *body) {
  sendHeaders(client, status, "application/json; charset=utf-8", strlen(body));
  client.print(body);
}

String getQueryParameter(const String &url, const char *key) {
  int q = url.indexOf('?');
  if (q < 0) return String("");

  String query = url.substring(q + 1);
  String token = String(key) + "=";
  int pos = query.indexOf(token);
  if (pos < 0) return String("");

  int start = pos + token.length();
  int end = query.indexOf('&', start);
  if (end < 0) end = query.length();
  return query.substring(start, end);
}

void sendDataJson(WiFiClient &client) {
  const float progress = currentProgress();
  const bool rightSide = filteredAngleDeg >= 0.0f;
  const int maxCount = rightSide ? 28 : 27;
  const int ledCount = (int)lroundf((progress / 100.0f) * maxCount);
  const bool freshSensor = sensorReady && lastImuMillis > 0 && (millis() - lastImuMillis < 1500UL);

  IPAddress ip = WiFi.localIP();
  char ipText[20];
  snprintf(ipText, sizeof(ipText), "%u.%u.%u.%u", (unsigned)ip[0], (unsigned)ip[1], (unsigned)ip[2], (unsigned)ip[3]);

  char json[560];
  snprintf(json, sizeof(json),
    "{\"ok\":true,\"sensorOk\":%s,\"sensor\":\"%s\",\"ip\":\"%s\","
    "\"angle\":%.2f,\"progress\":%.1f,\"targetAngle\":%d,\"holdMs\":%lu,"
    "\"ledCount\":%d,\"side\":\"%s\",\"samples\":%lu,\"uptimeMs\":%lu,"
    "\"raw\":{\"ax\":%.4f,\"ay\":%.4f,\"az\":%.4f,"
    "\"gx\":%.3f,\"gy\":%.3f,\"gz\":%.3f,\"neutralAngle\":%.2f}}",
    freshSensor ? "true" : "false", SENSOR_NAME, ipText,
    filteredAngleDeg, progress, targetAngleDeg, (unsigned long)holdDurationMs,
    ledCount, rightSide ? "right" : "left", (unsigned long)sampleCounter,
    (unsigned long)millis(), rawAx, rawAy, rawAz, rawGx, rawGy, rawGz, neutralAngleDeg
  );

  sendJson(client, "200 OK", json);
}

void handleHttpClient(WiFiClient &client) {
  client.setTimeout(350);
  String requestLine = client.readStringUntil('\n');
  requestLine.trim();

  // Consume request headers; requests from the supplied page are simple GETs.
  uint32_t headerStart = millis();
  while (client.connected() && (millis() - headerStart) < 500UL) {
    if (client.available()) {
      String headerLine = client.readStringUntil('\n');
      if (headerLine == "\r" || headerLine.length() == 0) break;
    } else {
      delay(1);
    }
  }

  int firstSpace = requestLine.indexOf(' ');
  int secondSpace = requestLine.indexOf(' ', firstSpace + 1);
  if (firstSpace < 0 || secondSpace < 0) {
    sendJson(client, "400 Bad Request", "{\"ok\":false,\"error\":\"Bad request\"}");
    client.stop();
    return;
  }

  String method = requestLine.substring(0, firstSpace);
  String url = requestLine.substring(firstSpace + 1, secondSpace);

  if (method == "OPTIONS") {
    sendHeaders(client, "204 No Content", "text/plain", 0);
  } else if (url == "/" || url.startsWith("/?")) {
    sendHeaders(client, "200 OK", "text/html; charset=utf-8", strlen(INDEX_HTML));
    client.write((const uint8_t *)INDEX_HTML, strlen(INDEX_HTML));
  } else if (url.startsWith("/api/data")) {
    sendDataJson(client);
  } else if (url.startsWith("/api/config")) {
    String targetValue = getQueryParameter(url, "target");
    String holdValue = getQueryParameter(url, "hold");
    if (targetValue.length() > 0) {
      int v = targetValue.toInt();
      if (v < 5) v = 5;
      if (v > 60) v = 60;
      targetAngleDeg = v;
    }
    if (holdValue.length() > 0) {
      long v = holdValue.toInt();
      if (v < 500) v = 500;
      if (v > 15000) v = 15000;
      holdDurationMs = (uint32_t)v;
    }
    char json[160];
    snprintf(json, sizeof(json), "{\"ok\":true,\"targetAngle\":%d,\"holdMs\":%lu}", targetAngleDeg, (unsigned long)holdDurationMs);
    sendJson(client, "200 OK", json);
  } else if (url.startsWith("/api/calibrate")) {
    calibrateNeutral();
    char json[130];
    snprintf(json, sizeof(json), "{\"ok\":true,\"message\":\"Nulova poloha byla nastavena.\",\"neutralAngle\":%.2f}", neutralAngleDeg);
    sendJson(client, "200 OK", json);
  } else if (url.startsWith("/api/health")) {
    sendJson(client, "200 OK", "{\"ok\":true,\"device\":\"Nano RP2040 Connect\"}");
  } else {
    sendJson(client, "404 Not Found", "{\"ok\":false,\"error\":\"Not found\"}");
  }

  delay(2);
  client.stop();
}

// -----------------------------------------------------------------------------
// WIFI STARTUP / RETRY
// -----------------------------------------------------------------------------

void startAccessPoint() {
  Serial.println(F("Starting Wi-Fi access point..."));
  int status = WiFi.beginAP(AP_SSID, AP_PASSWORD, AP_CHANNEL);
  if (status == WL_AP_LISTENING) {
    delay(700);
    server.begin();
    wifiReady = true;
    Serial.println(F("Wi-Fi AP ready."));
    Serial.print(F("SSID: ")); Serial.println(AP_SSID);
    Serial.print(F("Password: ")); Serial.println(AP_PASSWORD);
    Serial.print(F("Open in browser: http://")); Serial.println(WiFi.localIP());
  } else {
    wifiReady = false;
    Serial.print(F("Wi-Fi AP start failed. Status: "));
    Serial.println(status);
  }
}

// -----------------------------------------------------------------------------
// SETUP / LOOP
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.println(F("===================================="));
  Serial.println(F("       REHAB MOTION LAB"));
  Serial.println(F(" Arduino Nano RP2040 Connect"));
  Serial.println(F("===================================="));

  strip.neoPixelClear(true);

#if USE_EXTERNAL_MPU6050
  Serial.println(F("Initializing external MPU6050 over I2C..."));
#else
  Serial.println(F("Initializing built-in LSM6DSOX IMU..."));
#endif
  if (beginMotionSensor()) {
    sensorReady = true;
    Serial.print(SENSOR_NAME); Serial.println(F(" initialized."));
    Serial.println(F("Keep the bar still in neutral position for calibration."));
    delay(300);
    calibrateNeutral();
  } else {
    sensorReady = false;
    Serial.print(F("ERROR: Could not initialize ")); Serial.println(SENSOR_NAME);
    Serial.println(F("The web page will still open, but sensor data will be unavailable."));
  }

  lastImuMicros = micros();
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("ERROR: NINA Wi-Fi module not detected. Check WiFiNINA firmware."));
  } else {
    startAccessPoint();
  }
}

void loop() {
  updateSensor();
  updateLedStrip();

  if (wifiReady) {
    WiFiClient client = server.available();
    if (client) handleHttpClient(client);
  } else if (millis() - lastWifiRetryMs > 8000UL) {
    lastWifiRetryMs = millis();
    if (WiFi.status() != WL_NO_MODULE) startAccessPoint();
  }
}
