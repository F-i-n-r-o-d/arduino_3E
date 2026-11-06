const http = require('http');
 
const server = http.createServer((req, res) => {
  // Sledujeme, jestli přijde POST požadavek na správnou adresu
  if (req.method === 'POST' && req.url === '/api/score') {
    let body = '';
   
    // Čteme příchozí data (JSON z ESP32)
    req.on('data', chunk => {
      body += chunk.toString();
    });
 
    // Až jsou data kompletní
    req.on('end', () => {
      try {
        const data = JSON.parse(body);
        console.log("\n--- PŘIJATO Z TYČE ---");
        console.log(`Hra: ${data.game}`);
        console.log(`Skóre (přihrávek): ${data.score}`);
        console.log(`Stav: ${data.status}`);
       
        // Odpovíme ESP32, že vše proběhlo OK
        res.writeHead(200, { 'Content-Type': 'text/plain' });
        res.end('OK');
      } catch (e) {
        res.writeHead(400, { 'Content-Type': 'text/plain' });
        res.end('Chyba parsování JSONu');
      }
    });
  } else {
    res.writeHead(404, { 'Content-Type': 'text/plain' });
    res.end('Stránka nenalezena');
  }
});
 
// Spuštění serveru na portu 3000
server.listen(3000, '0.0.0.0', () => {
  console.log('Node.js server běží na portu 3000 a čeká na data z tyče...');
});
 