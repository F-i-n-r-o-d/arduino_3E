// Spuštění:  npm install   &&   npm start
// Pak otevři  http://localhost:3000  (jiná místnost: http://localhost:3000/?room=test)
const http = require('http');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { WebSocketServer } = require('ws');

const PORT = process.env.PORT || 3000;
const WIN = 10;
const rooms = new Map();

function getRoom(name) {
  if (!rooms.has(name)) rooms.set(name, { name, state: 'waiting', winner: null, epoch: 0, solo: false, players: new Map(), viewers: new Set() });
  return rooms.get(name);
}
const connectedCount = r => [...r.players.values()].filter(p => p.ws).length;
const send = (ws, o) => { if (ws && ws.readyState === 1) ws.send(JSON.stringify(o)); };
const log = (room, text) => console.log(`${new Date().toLocaleTimeString('cs-CZ')}  [${room.name}] ${text}`);

function broadcast(room) {
  for (const [id, p] of room.players) {
    const other = [...room.players].find(([k]) => k !== id);
    const opp = other && other[1];
    send(p.ws, {
      type: 'status',
      state: room.state,                 // waiting | playing | finished
      epoch: room.epoch,                 // zvyšuje se při každé nové hře
      solo: room.solo,
      won: room.winner === null ? null : room.winner === id,
      you: p.score,
      opp: opp ? opp.score : 0,
      oppConnected: !!(opp && opp.ws),
    });
  }
  sendMonitor(room);
}

// přehled pro webovou stránku (monitor)
function sendMonitor(room) {
  if (!room.viewers.size) return;
  const msg = JSON.stringify({
    type: 'monitor', state: room.state, epoch: room.epoch, solo: room.solo,
    players: [...room.players].map(([id, p]) => ({
      id, score: p.score, connected: !!p.ws, pos: p.pos, cil: p.cil, winner: room.winner === id,
    })),
  });
  for (const v of room.viewers) if (v.readyState === 1) v.send(msg);
}

// nová hra: duel, když jsou připojené dvě tyče, sólo, když běželo sólo a tyč je jedna, jinak čekání
function resetRoom(room) {
  const n = connectedCount(room);
  if (n === 2) room.solo = false;
  for (const p of room.players.values()) p.score = 0;
  room.winner = null;
  room.epoch++;
  room.state = n === 2 || (room.solo && n === 1) ? 'playing' : 'waiting';
  if (room.state === 'waiting') room.solo = false;
  broadcast(room);
}

// sólo hra – jen když je připojená právě jedna tyč a nic neběží (učitel má jen jednu tyč)
function startSolo(room) {
  if (room.state !== 'waiting' || connectedCount(room) !== 1) return;
  for (const [k, q] of room.players) if (!q.ws) room.players.delete(k);   // pryč s dříve odpojenými tyčemi
  room.solo = true;
  resetRoom(room);
  log(room, 'sólo hra spuštěna');
}

// ---------- HTTP: stránky + REST API ----------
const server = http.createServer((req, res) => {
  const url = new URL(req.url, 'http://x');
  const m = url.pathname.match(/^\/api\/rooms\/([^/]+)(?:\/(reset|solo))?$/);
  if (m) {
    const room = getRoom(decodeURIComponent(m[1]));
    if (req.method === 'POST' && m[2] === 'reset') resetRoom(room);
    else if (req.method === 'POST' && m[2] === 'solo') startSolo(room);
    else if (req.method !== 'GET') { res.writeHead(405); return res.end(); }
    res.writeHead(200, { 'Content-Type': 'application/json' });
    return res.end(JSON.stringify({
      state: room.state, epoch: room.epoch, solo: room.solo,
      players: [...room.players].map(([id, p]) => ({ id, score: p.score, connected: !!p.ws })),
    }));
  }
  const pages = { '/': 'index.html', '/tyc': 'tyc.html' };   // /tyc = virtuální tyče pro testování bez hardwaru
  if (pages[url.pathname]) {
    res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
    return fs.createReadStream(path.join(__dirname, pages[url.pathname])).pipe(res);
  }
  res.writeHead(404); res.end();
});

// ---------- WebSocket ----------
const wss = new WebSocketServer({ server });

wss.on('connection', (ws, req) => {
  let room = null, id = null, viewerOf = null;
  const odkud = `${(req.socket.remoteAddress || '').replace('::ffff:', '')}, ${/arduino/i.test(req.headers['user-agent'] || '') ? 'Arduino' : 'prohlížeč'}`;
  ws.alive = true;
  ws.on('pong', () => (ws.alive = true));

  ws.on('message', raw => {
    let m; try { m = JSON.parse(raw); } catch { return; }

    if (m.type === 'join' && m.role === 'viewer') {
      viewerOf = getRoom(String(m.room || 'default').slice(0, 32));
      viewerOf.viewers.add(ws);
      return sendMonitor(viewerOf);
    }
    if (m.type === 'join') {
      const r = getRoom(String(m.room || 'default').slice(0, 32));
      const pid = String(m.id || '').slice(0, 64);
      if (!pid) return;
      let p = r.players.get(pid);
      if (!p) {
        if (r.players.size >= 2) {               // místo uvolní tyč, která už není připojená
          const odpojena = [...r.players].find(([, q]) => !q.ws);
          if (!odpojena) { log(r, `${pid} odmítnuta – už hrají dvě tyče`); send(ws, { type: 'full' }); return ws.close(); }
          r.players.delete(odpojena[0]);
        }
        p = { score: 0, ws: null, pos: null, cil: null };
        r.players.set(pid, p);
      }
      if (p.ws && p.ws !== ws) { send(p.ws, { type: 'kicked' }); p.ws.close(); }   // opětovné připojení stejného hráče
      p.ws = ws; room = r; id = pid;
      log(r, `${pid} připojena (${odkud})`);
      if (connectedCount(r) === 2 && r.solo) {   // druhá tyč ukončí sólo => duel od nuly
        log(r, 'připojila se druhá tyč – začíná duel');
        return resetRoom(r);
      }
      if (r.state === 'waiting' && connectedCount(r) === 2) r.state = 'playing';
      broadcast(r);
      return;
    }
    if (!room) return;
    const p = room.players.get(id);
    if (!p || p.ws !== ws) return;

    if (m.type === 'pos') {
      p.pos = Number(m.pos);
      if (m.cil !== undefined) p.cil = Number(m.cil);   // žlutý pás – jen pro web, soupeři se neposílá
      for (const [k, q] of room.players) if (k !== id) send(q.ws, { type: 'opp', pos: p.pos });
    } else if (m.type === 'score' && room.state === 'playing' && Number(m.epoch) === room.epoch) {
      const s = Math.min(WIN, Math.floor(Number(m.score)));
      if (s > p.score) {                         // absolutní hodnota => odolné proti výpadkům
        p.score = s;
        log(room, `${id}: ${s} ${s === 1 ? 'bod' : s < 5 ? 'body' : 'bodů'}`);
        if (s >= WIN) { room.state = 'finished'; room.winner = id; log(room, `${id} splnila ${WIN} úkolů – konec hry`); }
        broadcast(room);
      }
    } else if (m.type === 'rematch' && room.state === 'finished') {
      resetRoom(room);
      log(room, 'nová hra');
    } else if (m.type === 'solo') {
      startSolo(room);
    }
  });

  ws.on('close', () => {
    if (viewerOf) viewerOf.viewers.delete(ws);
    if (!room) return;
    const p = room.players.get(id);
    if (p && p.ws === ws) { p.ws = null; log(room, `${id} odpojena`); broadcast(room); }
  });
});

// web dostává polohy 20× za sekundu => plynulý pohyb kuličky
setInterval(() => { for (const r of rooms.values()) sendMonitor(r); }, 50);

// detekce mrtvých spojení
setInterval(() => {
  for (const ws of wss.clients) {
    if (!ws.alive) { ws.terminate(); continue; }
    ws.alive = false; ws.ping();
  }
}, 5000);

server.listen(PORT, () => {
  console.log('Server běží na http://localhost:' + PORT);
  // IP adresy, které se zadávají do tyc1.ino / tyc2.ino (SERVER_IP)
  for (const [name, list] of Object.entries(os.networkInterfaces()))
    for (const a of list) if (a.family === 'IPv4' && !a.internal) console.log(`  IP pro tyče: ${a.address}  (${name})`);
});
