'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const { createServer } = require('../../lab2/server/server');

const GUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';
const clients = new Set();

function frame(object) {
  const payload = Buffer.from(JSON.stringify(object));
  if (payload.length < 126) return Buffer.concat([Buffer.from([0x81, payload.length]), payload]);
  const header = Buffer.alloc(4);
  header[0] = 0x81;
  header[1] = 126;
  header.writeUInt16BE(payload.length, 2);
  return Buffer.concat([header, payload]);
}

function broadcast(type, message) {
  const packet = frame({
    type,
    message,
    instance: process.env.INSTANCE_ID || 'backend-local',
    version: process.env.APP_VERSION || 'blue-1.0.0',
    time: new Date().toISOString()
  });
  for (const socket of clients) {
    if (socket.writable && !socket.destroyed) socket.write(packet);
    else clients.delete(socket);
  }
}

function attachPush(server) {
  server.on('upgrade', (req, socket) => {
    const pathname = new URL(req.url, 'http://localhost').pathname;
    const key = req.headers['sec-websocket-key'];
    if (pathname !== '/ws/notifications' || !key) return socket.destroy();
    const accept = crypto.createHash('sha1').update(key + GUID).digest('base64');
    socket.write([
      'HTTP/1.1 101 Switching Protocols',
      'Upgrade: websocket',
      'Connection: Upgrade',
      'Sec-WebSocket-Accept: ' + accept,
      '\r\n'
    ].join('\r\n'));
    clients.add(socket);
    socket.write(frame({
      type: 'connected',
      message: 'Push-канал подключён',
      instance: process.env.INSTANCE_ID || 'backend-local',
      version: process.env.APP_VERSION || 'blue-1.0.0',
      time: new Date().toISOString()
    }));
    socket.on('close', () => clients.delete(socket));
    socket.on('error', () => clients.delete(socket));
    socket.on('data', (data) => {
      if (data.length > 0 && (data[0] & 0x0f) === 0x8) {
        clients.delete(socket);
        socket.end();
      }
    });
  });
}

// Файл данных общий для всех экземпляров, поэтому изменение, сделанное через
// любой backend, видят все остальные и рассылают push своим клиентам.
function watchCollection(dataFile) {
  fs.watchFile(dataFile, { interval: 1000 }, (current, previous) => {
    if (current.mtimeMs === previous.mtimeMs) return;
    let count = '?';
    try { count = JSON.parse(fs.readFileSync(dataFile, 'utf8')).items.length; }
    catch (_) { /* файл может быть в процессе замены */ }
    broadcast('albums', `Коллекция обновлена: альбомов ${count}`);
  }).unref();
}

const useHttps = process.env.HTTPS === '1';
const port = Number(process.env.PORT) || 9101;
const dataFile = process.env.DATA_FILE || path.join(__dirname, 'data', 'albums.json');
const server = createServer({
  https: useHttps,
  secureCookie: useHttps,
  dataFile,
  keyFile: process.env.TLS_KEY,
  certFile: process.env.TLS_CERT
});

server.prependListener('request', (req, res) => {
  res.setHeader('X-Instance-Id', process.env.INSTANCE_ID || 'backend-local');
});
attachPush(server);
watchCollection(dataFile);
server.listen(port, process.env.HOST || '0.0.0.0', () => {
  console.log(`Instance ${process.env.INSTANCE_ID || 'local'} listens on ${port}`);
  broadcast('deployment', 'Экземпляр запущен');
});

setInterval(() => broadcast('heartbeat', 'Сервис доступен'), 15000).unref();

module.exports = { attachPush, broadcast, watchCollection };
