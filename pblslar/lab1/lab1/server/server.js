// Лабораторная работа №1. Вариант 5 (номер в списке 19).
// Сервер отдаёт web-клиент, REST API и WebSocket API для клиента ОС Аврора.

const http = require('http');
const fs = require('fs');
const path = require('path');
const os = require('os');
const crypto = require('crypto');

const PORT = Number(process.env.PORT) || 8085;
const HOST = process.env.HOST || '0.0.0.0';
const PUBLIC_DIR = path.join(__dirname, 'public');
const WEBSOCKET_GUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';
const MAX_BODY_SIZE = 10_000;
const MAX_WS_MESSAGE_SIZE = 65_536;

const MIME = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.ico': 'image/x-icon',
};

const history = [];
const HISTORY_LIMIT = 10;

class InputError extends Error {}

function convert(value, from) {
  if (from === 'C') return { value: value * 9 / 5 + 32, unit: 'F' };
  if (from === 'F') return { value: (value - 32) * 5 / 9, unit: 'C' };
  throw new InputError('Единица измерения должна быть C или F');
}

function round(value) {
  return Math.round(value * 100) / 100;
}

function createRecord(rawValue, rawFrom) {
  const value = Number(rawValue);
  const from = String(rawFrom || '').toUpperCase();

  if (rawValue === '' || rawValue === null || rawValue === undefined || !Number.isFinite(value)) {
    throw new InputError('Введите корректное число');
  }
  if (from !== 'C' && from !== 'F') {
    throw new InputError('Единица измерения должна быть C или F');
  }
  if ((from === 'C' && value < -273.15) || (from === 'F' && value < -459.67)) {
    throw new InputError('Температура ниже абсолютного нуля невозможна');
  }

  const result = convert(value, from);
  const record = {
    input: value,
    from,
    output: round(result.value),
    to: result.unit,
    time: new Date().toISOString(),
  };

  history.unshift(record);
  if (history.length > HISTORY_LIMIT) history.pop();
  console.log('[convert] ' + value + '°' + from + ' -> ' + record.output + '°' + record.to);
  return record;
}

function sendJson(res, status, data) {
  res.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Cache-Control': 'no-store',
  });
  res.end(JSON.stringify(data));
}

function readBody(req) {
  return new Promise((resolve, reject) => {
    let body = '';
    req.on('data', (chunk) => {
      body += chunk;
      if (body.length > MAX_BODY_SIZE) {
        reject(new InputError('Слишком большой запрос'));
        req.destroy();
      }
    });
    req.on('end', () => resolve(body));
    req.on('error', reject);
  });
}

function serveStatic(req, res) {
  let urlPath;
  try {
    urlPath = decodeURIComponent(req.url.split('?')[0]);
  } catch (_) {
    res.writeHead(400, { 'Content-Type': 'text/plain; charset=utf-8' });
    return res.end('Некорректный URL');
  }

  const relativePath = urlPath === '/' ? 'index.html' : urlPath.replace(/^[/\\]+/, '');
  const filePath = path.resolve(PUBLIC_DIR, relativePath);
  if (filePath !== PUBLIC_DIR && !filePath.startsWith(PUBLIC_DIR + path.sep)) {
    res.writeHead(403, { 'Content-Type': 'text/plain; charset=utf-8' });
    return res.end('Forbidden');
  }

  fs.readFile(filePath, (error, data) => {
    if (error) {
      res.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
      return res.end('404 — файл не найден');
    }
    res.writeHead(200, {
      'Content-Type': MIME[path.extname(filePath)] || 'application/octet-stream',
      'X-Content-Type-Options': 'nosniff',
    });
    res.end(data);
  });
}

async function handleConvert(req, res) {
  try {
    const payload = JSON.parse(await readBody(req));
    sendJson(res, 200, createRecord(payload.value, payload.from));
  } catch (error) {
    const status = error instanceof InputError || error instanceof SyntaxError ? 400 : 500;
    sendJson(res, status, { error: error instanceof SyntaxError ? 'Некорректный JSON' : error.message });
  }
}

function encodeWebSocketFrame(data, opcode = 0x1) {
  const payload = Buffer.from(data);
  let header;
  if (payload.length < 126) {
    header = Buffer.from([0x80 | opcode, payload.length]);
  } else if (payload.length <= 0xffff) {
    header = Buffer.alloc(4);
    header[0] = 0x80 | opcode;
    header[1] = 126;
    header.writeUInt16BE(payload.length, 2);
  } else {
    header = Buffer.alloc(10);
    header[0] = 0x80 | opcode;
    header[1] = 127;
    header.writeBigUInt64BE(BigInt(payload.length), 2);
  }
  return Buffer.concat([header, payload]);
}

function sendWs(socket, data) {
  if (!socket.destroyed && socket.writable) {
    socket.write(encodeWebSocketFrame(JSON.stringify(data)));
  }
}

function closeWs(socket, code, reason) {
  if (socket.destroyed) return;
  const reasonBuffer = Buffer.from(reason || '');
  const payload = Buffer.alloc(2 + reasonBuffer.length);
  payload.writeUInt16BE(code, 0);
  reasonBuffer.copy(payload, 2);
  socket.end(encodeWebSocketFrame(payload, 0x8));
}

function processWsMessage(socket, text) {
  let message;
  try {
    message = JSON.parse(text);
  } catch (_) {
    return sendWs(socket, { type: 'error', message: 'Некорректный JSON' });
  }

  try {
    switch (message.type) {
      case 'convert':
        sendWs(socket, { type: 'result', record: createRecord(message.value, message.from) });
        sendWs(socket, { type: 'history', items: history });
        break;
      case 'history':
        sendWs(socket, { type: 'history', items: history });
        break;
      case 'clearHistory':
        history.length = 0;
        sendWs(socket, { type: 'history', items: history });
        break;
      case 'ping':
        sendWs(socket, { type: 'pong', time: new Date().toISOString() });
        break;
      default:
        sendWs(socket, { type: 'error', message: 'Неизвестный тип сообщения' });
    }
  } catch (error) {
    sendWs(socket, { type: 'error', message: error.message });
  }
}

function consumeWebSocketFrames(socket, state) {
  while (state.buffer.length >= 2) {
    const first = state.buffer[0];
    const second = state.buffer[1];
    const fin = Boolean(first & 0x80);
    const opcode = first & 0x0f;
    const masked = Boolean(second & 0x80);
    let length = second & 0x7f;
    let offset = 2;

    if (!fin) return closeWs(socket, 1003, 'Фрагментированные сообщения не поддерживаются');
    if (!masked) return closeWs(socket, 1002, 'Клиентское сообщение должно быть маскировано');
    if (length === 126) {
      if (state.buffer.length < 4) return;
      length = state.buffer.readUInt16BE(2);
      offset = 4;
    } else if (length === 127) {
      if (state.buffer.length < 10) return;
      const largeLength = state.buffer.readBigUInt64BE(2);
      if (largeLength > BigInt(MAX_WS_MESSAGE_SIZE)) return closeWs(socket, 1009, 'Сообщение слишком большое');
      length = Number(largeLength);
      offset = 10;
    }
    if (length > MAX_WS_MESSAGE_SIZE) return closeWs(socket, 1009, 'Сообщение слишком большое');
    if (state.buffer.length < offset + 4 + length) return;

    const mask = state.buffer.subarray(offset, offset + 4);
    offset += 4;
    const payload = Buffer.from(state.buffer.subarray(offset, offset + length));
    for (let i = 0; i < payload.length; i += 1) payload[i] ^= mask[i % 4];
    state.buffer = state.buffer.subarray(offset + length);

    if (opcode === 0x8) return closeWs(socket, 1000, '');
    if (opcode === 0x9) {
      socket.write(encodeWebSocketFrame(payload, 0xA));
      continue;
    }
    if (opcode !== 0x1) return closeWs(socket, 1003, 'Поддерживаются только текстовые сообщения');
    processWsMessage(socket, payload.toString('utf8'));
  }
}

function handleUpgrade(req, socket, head) {
  const pathname = req.url.split('?')[0];
  const key = req.headers['sec-websocket-key'];
  if (pathname !== '/ws' || !key || String(req.headers.upgrade).toLowerCase() !== 'websocket') {
    socket.write('HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n');
    return socket.destroy();
  }

  const accept = crypto.createHash('sha1').update(key + WEBSOCKET_GUID).digest('base64');
  socket.write([
    'HTTP/1.1 101 Switching Protocols',
    'Upgrade: websocket',
    'Connection: Upgrade',
    'Sec-WebSocket-Accept: ' + accept,
    '\r\n',
  ].join('\r\n'));

  const state = { buffer: head && head.length ? Buffer.from(head) : Buffer.alloc(0) };
  sendWs(socket, { type: 'connected', message: 'Соединение установлено' });
  sendWs(socket, { type: 'history', items: history });
  if (state.buffer.length) consumeWebSocketFrames(socket, state);
  socket.on('data', (chunk) => {
    state.buffer = Buffer.concat([state.buffer, chunk]);
    consumeWebSocketFrames(socket, state);
  });
  socket.on('error', (error) => console.error('[ws] ' + error.message));
}

const server = http.createServer((req, res) => {
  const url = req.url.split('?')[0];
  if (url === '/api/convert' && req.method === 'POST') return handleConvert(req, res);
  if (url === '/api/history' && req.method === 'GET') return sendJson(res, 200, history);
  if (url === '/api/history' && req.method === 'DELETE') {
    history.length = 0;
    return sendJson(res, 200, { ok: true });
  }
  if (req.method === 'GET') return serveStatic(req, res);
  sendJson(res, 405, { error: 'Метод не поддерживается' });
});

server.on('upgrade', handleUpgrade);

if (require.main === module) {
  server.listen(PORT, HOST, () => {
    console.log('Сервер преобразователя температуры запущен.');
    console.log('  Web:       http://localhost:' + PORT);
    console.log('  WebSocket: ws://localhost:' + PORT + '/ws');
    const nets = os.networkInterfaces();
    Object.keys(nets).forEach((name) => {
      nets[name].forEach((net) => {
        if (net.family === 'IPv4' && !net.internal) {
          console.log('  ОС Аврора: ws://' + net.address + ':' + PORT + '/ws   (' + name + ')');
        }
      });
    });
  });
}

module.exports = { server, convert, createRecord, history };
