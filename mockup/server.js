const http = require('http');
const fs = require('fs');
const path = require('path');
const server = http.createServer((req, res) => {
  const filePath = path.join(__dirname, req.url === '/' ? 'ai-eq-pro-mockup.html' : req.url);
  fs.readFile(filePath, (err, data) => {
    if (err) { res.writeHead(404); res.end('Not found'); return; }
    res.writeHead(200, {'Content-Type': 'text/html'});
    res.end(data);
  });
});
server.listen(8090, () => console.log('Server running on port 8090'));
