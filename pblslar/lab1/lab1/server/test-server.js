const assert = require('assert');
const http = require('http');
const net = require('net');
const crypto = require('crypto');
const { server, history } = require('./server');

function httpJson(port, method, pathname, body) {
  return new Promise((resolve, reject) => {
    const payload = body === undefined ? null : JSON.stringify(body);
    const request = http.request({
      host: '127.0.0.1',
      port,
      method,
      path: pathname,
      headers: payload ? {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(payload),
      } : {},
    }, (response) => {
      let text = '';
      response.setEncoding('utf8');
      response.on('data', (chunk) => { text += chunk; });
      response.on('end', () => resolve({ status: response.statusCode, body: JSON.parse(text) }));
    });
    request.on('error', reject);
    if (payload) request.write(payload);
    request.end();
  });
}

function maskedTextFrame(text) {
  const payload = Buffer.from(text);
  const mask = crypto.randomBytes(4);
  const header = payload.length < 126
    ? Buffer.from([0x81, 0x80 | payload.length])
    : Buffer.from([0x81, 0x80 | 126, payload.length >> 8, payload.length & 0xff]);
  const encoded = Buffer.from(payload);
  for (let i = 0; i < encoded.length; i += 1) encoded[i] ^= mask[i % 4];
  return Buffer.concat([header, mask, encoded]);
}

function parseServerFrames(buffer) {
  const messages = [];
  while (buffer.length >= 2) {
    let length = buffer[1] & 0x7f;
    let offset = 2;
    if (length === 126) {
      if (buffer.length < 4) break;
      length = buffer.readUInt16BE(2);
      offset = 4;
    }
    if (buffer.length < offset + length) break;
    if ((buffer[0] & 0x0f) === 0x1) {
      messages.push(JSON.parse(buffer.subarray(offset, offset + length).toString('utf8')));
    }
    buffer = buffer.subarray(offset + length);
  }
  return { messages, rest: buffer };
}

function websocketConvert(port) {
  return new Promise((resolve, reject) => {
    const socket = net.createConnection({ host: '127.0.0.1', port });
    let handshakeDone = false;
    let buffer = Buffer.alloc(0);
    const timer = setTimeout(() => {
      socket.destroy();
      reject(new Error('Тайм-аут проверки WebSocket'));
    }, 3000);

    socket.on('connect', () => {
      const key = crypto.randomBytes(16).toString('base64');
      socket.write([
        'GET /ws HTTP/1.1',
        'Host: 127.0.0.1:' + port,
        'Upgrade: websocket',
        'Connection: Upgrade',
        'Sec-WebSocket-Key: ' + key,
        'Sec-WebSocket-Version: 13',
        '\r\n',
      ].join('\r\n'));
    });

    socket.on('data', (chunk) => {
      buffer = Buffer.concat([buffer, chunk]);
      if (!handshakeDone) {
        const marker = buffer.indexOf('\r\n\r\n');
        if (marker < 0) return;
        const header = buffer.subarray(0, marker).toString('utf8');
        assert.match(header, /101 Switching Protocols/);
        buffer = buffer.subarray(marker + 4);
        handshakeDone = true;
        socket.write(maskedTextFrame(JSON.stringify({ type: 'convert', value: 212, from: 'F' })));
      }

      const parsed = parseServerFrames(buffer);
      buffer = parsed.rest;
      const result = parsed.messages.find((message) => message.type === 'result');
      if (result) {
        clearTimeout(timer);
        socket.destroy();
        resolve(result.record);
      }
    });
    socket.on('error', reject);
  });
}

async function run() {
  history.length = 0;
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  const port = server.address().port;
  try {
    const converted = await httpJson(port, 'POST', '/api/convert', { value: 0, from: 'C' });
    assert.strictEqual(converted.status, 200);
    assert.strictEqual(converted.body.output, 32);
    assert.strictEqual(converted.body.to, 'F');

    const invalid = await httpJson(port, 'POST', '/api/convert', { value: -500, from: 'C' });
    assert.strictEqual(invalid.status, 400);

    const wsResult = await websocketConvert(port);
    assert.strictEqual(wsResult.output, 100);
    assert.strictEqual(wsResult.to, 'C');
    console.log('OK: REST API и WebSocket API работают корректно');
  } finally {
    await new Promise((resolve) => server.close(resolve));
  }
}

run().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
