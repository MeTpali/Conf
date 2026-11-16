'use strict';
// Мини-клиент WebSocket без зависимостей: печатает push-уведомления.
// node scripts/watch-push.js [ws://localhost:8080/ws/notifications]
const crypto = require('crypto');
const net = require('net');

const url = new URL(process.argv[2] || 'ws://localhost:8080/ws/notifications');
const socket = net.connect(Number(url.port) || 80, url.hostname, () => {
  socket.write(`GET ${url.pathname} HTTP/1.1\r\nHost: ${url.host}\r\nUpgrade: websocket\r\n` +
    `Connection: Upgrade\r\nSec-WebSocket-Key: ${crypto.randomBytes(16).toString('base64')}\r\n` +
    'Sec-WebSocket-Version: 13\r\n\r\n');
});
let buffer = Buffer.alloc(0);
let upgraded = false;
socket.on('data', (chunk) => {
  buffer = Buffer.concat([buffer, chunk]);
  if (!upgraded) {
    const end = buffer.indexOf('\r\n\r\n');
    if (end < 0) return;
    upgraded = buffer.slice(0, 12).toString() === 'HTTP/1.1 101';
    if (!upgraded) { console.error('Handshake failed'); process.exit(1); }
    buffer = buffer.slice(end + 4);
  }
  while (buffer.length >= 2) {
    let length = buffer[1] & 0x7f;
    let offset = 2;
    if (length === 126) { if (buffer.length < 4) return; length = buffer.readUInt16BE(2); offset = 4; }
    if (buffer.length < offset + length) return;
    console.log(buffer.slice(offset, offset + length).toString());
    buffer = buffer.slice(offset + length);
  }
});
socket.on('close', () => { console.log('Соединение закрыто'); process.exit(0); });
