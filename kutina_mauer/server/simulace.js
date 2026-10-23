// Cvičný běh bez hardwaru: spustí server a dvě virtuální tyče, které hrají proti sobě.
// Spuštění:  npm run simulace   a pak otevři  http://localhost:3000
// Virtuální tyče mluví se serverem stejnými zprávami jako tyc1.ino / tyc2.ino.
require('./server');
const WebSocket = require('ws');

const URL = 'ws://localhost:' + (process.env.PORT || 3000);
const ROOM = 'rehab';
const LED = 64, SIRKA_CILE = 10, VITEZ = 10, DOBA_V_CILI = 1500;

function virtualniTyc(id, rychlost, vypadekPriSkore) {
  let ws = null, epoch = -1, state = 'waiting', score = 0;
  let pos = 32, cil = 0, vCiliOd = 0, offline = false;
  const send = o => { if (ws && ws.readyState === 1) ws.send(JSON.stringify(o)); };
  const log = t => console.log(`[${id}] ${t}`);

  function novyCil() {
    do { cil = Math.floor(Math.random() * (LED - SIRKA_CILE + 1)); } while (pos >= cil && pos < cil + SIRKA_CILE);
    vCiliOd = 0;
  }
  novyCil();

  function pripoj() {
    ws = new WebSocket(URL);
    ws.on('open', () => { log('připojeno'); send({ type: 'join', room: ROOM, id }); });
    ws.on('message', raw => {
      const m = JSON.parse(raw);
      if (m.type !== 'status') return;
      if (m.epoch !== epoch) {
        if (epoch !== -1) { score = 0; novyCil(); log('nová hra'); }
        epoch = m.epoch;
      }
      if (m.you > score) score = m.you;
      if (score > m.you && m.state === 'playing') send({ type: 'score', score, epoch });   // dohnání po výpadku
      if (m.state === 'finished' && state !== 'finished') {
        log(m.won ? 'VYHRÁL!' : 'prohrál');
        setTimeout(() => send({ type: 'rematch' }), 5000);   // za 5 s další hra
      }
      state = m.state;
    });
    ws.on('close', () => { if (!offline) setTimeout(pripoj, 1000); });
    ws.on('error', () => {});
  }
  pripoj();

  // herní smyčka – stejná logika jako v .ino, jen místo náklonu "ruka" míří k cíli
  setInterval(() => {
    const stred = cil + SIRKA_CILE / 2;
    pos += Math.sign(stred - pos) * rychlost + (Math.random() - 0.5) * 2.5;
    pos = Math.max(0, Math.min(LED - 1, pos));
    const led = Math.round(pos);

    if (state === 'playing' && score < VITEZ) {
      if (led >= cil && led < cil + SIRKA_CILE) {
        if (!vCiliOd) vCiliOd = Date.now();
        if (Date.now() - vCiliOd >= DOBA_V_CILI) {
          score++;
          log('skóre ' + score);
          send({ type: 'score', score, epoch });
          novyCil();
          if (score === vypadekPriSkore) vypadek();
        }
      } else vCiliOd = 0;
    }
    send({ type: 'pos', pos: led, cil });
  }, 50);

  // ukázka výpadku spojení: tyč 3 s hraje offline, pak se připojí a pošle skóre
  function vypadek() {
    log('--- simulovaný výpadek spojení (3 s) ---');
    offline = true;
    ws.terminate();
    setTimeout(() => { offline = false; pripoj(); }, 3000);
  }
}

virtualniTyc('tyc1', 0.9, null);
virtualniTyc('tyc2', 1.0, 4);
console.log('Simulace běží – otevři http://localhost:' + (process.env.PORT || 3000));
