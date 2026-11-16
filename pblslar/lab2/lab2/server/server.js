'use strict';

const crypto = require('crypto');
const fs = require('fs');
const http = require('http');
const https = require('https');
const os = require('os');
const path = require('path');

const PUBLIC_DIR = path.join(__dirname, 'public');
const OPENAPI_FILE = path.join(__dirname, 'openapi.json');
const DEFAULT_DATA_FILE = path.join(__dirname, 'data', 'albums.json');
const MAX_BODY_SIZE = 64 * 1024;

class HttpError extends Error {
  constructor(status, message) {
    super(message);
    this.status = status;
  }
}

class AlbumStore {
  constructor(fileName) {
    this.fileName = fileName;
    this.albums = [];
    this.nextId = 1;
    this.lastModified = new Date();
    this.fileStamp = 0;
    this.load();
  }

  load() {
    try {
      const parsed = JSON.parse(fs.readFileSync(this.fileName, 'utf8'));
      this.albums = Array.isArray(parsed.items) ? parsed.items : [];
      this.nextId = Number(parsed.nextId) || 1;
      this.lastModified = new Date(parsed.lastModified || Date.now());
      this.fileStamp = fs.statSync(this.fileName).mtimeMs;
    } catch (error) {
      if (error.code !== 'ENOENT') throw error;
      this.albums = [
        { id: 1, performer: 'Кино', album: 'Группа крови', year: 1988, genre: 'Рок' },
        { id: 2, performer: 'Daft Punk', album: 'Discovery', year: 2001, genre: 'Electronic' }
      ];
      this.nextId = 3;
      this.persist();
    }
  }

  persist() {
    this.lastModified = new Date();
    fs.mkdirSync(path.dirname(this.fileName), { recursive: true });
    // Своё имя временного файла у каждого процесса: несколько экземпляров (ЛР3)
    // пишут в один каталог, и общий .tmp один из них переименовал бы раньше другого.
    const temporary = `${this.fileName}.${process.pid}.tmp`;
    fs.writeFileSync(temporary, JSON.stringify({
      nextId: this.nextId,
      lastModified: this.lastModified.toISOString(),
      items: this.albums
    }, null, 2));
    fs.renameSync(temporary, this.fileName);
    this.fileStamp = fs.statSync(this.fileName).mtimeMs;
  }

  // Несколько экземпляров API (ЛР3) работают с одним файлом: перечитываем его,
  // если другой экземпляр уже записал изменения.
  refresh() {
    let stamp;
    try { stamp = fs.statSync(this.fileName).mtimeMs; }
    catch (_) { return; }
    if (stamp !== this.fileStamp) this.load();
  }

  list() { this.refresh(); return this.albums.map((item) => ({ ...item })); }
  find(id) { this.refresh(); return this.albums.find((item) => item.id === id); }

  create(payload) {
    this.refresh();
    const album = normalizeAlbum(payload, false);
    album.id = this.nextId++;
    this.albums.push(album);
    this.persist();
    return { ...album };
  }

  update(id, payload, partial) {
    this.refresh();
    const index = this.albums.findIndex((item) => item.id === id);
    if (index < 0) throw new HttpError(404, 'Альбом не найден');
    const merged = partial ? { ...this.albums[index], ...payload } : payload;
    const album = normalizeAlbum(merged, false);
    album.id = id;
    this.albums[index] = album;
    this.persist();
    return { ...album };
  }

  remove(id) {
    this.refresh();
    const index = this.albums.findIndex((item) => item.id === id);
    if (index < 0) throw new HttpError(404, 'Альбом не найден');
    this.albums.splice(index, 1);
    this.persist();
  }
}

function normalizeAlbum(payload, allowId) {
  if (!payload || typeof payload !== 'object' || Array.isArray(payload)) {
    throw new HttpError(400, 'Ожидается JSON-объект');
  }
  const album = {
    performer: String(payload.performer || '').trim(),
    album: String(payload.album || '').trim(),
    year: Number(payload.year),
    genre: String(payload.genre || '').trim()
  };
  if (!album.performer || !album.album || !album.genre) {
    throw new HttpError(400, 'Исполнитель, альбом и жанр обязательны');
  }
  if (!Number.isInteger(album.year) || album.year < 1800 || album.year > 2100) {
    throw new HttpError(400, 'Год должен быть целым числом от 1800 до 2100');
  }
  if (allowId && payload.id) album.id = Number(payload.id);
  return album;
}

function base64url(value) {
  return Buffer.from(value).toString('base64')
    .replace(/=/g, '').replace(/\+/g, '-').replace(/\//g, '_');
}

function issueSession(user, secret) {
  const payload = base64url(JSON.stringify({
    sub: user.username,
    role: user.role,
    exp: Date.now() + 8 * 60 * 60 * 1000
  }));
  const signature = base64url(crypto.createHmac('sha256', secret).update(payload).digest());
  return payload + '.' + signature;
}

function parseSession(token, secret) {
  if (!token || !token.includes('.')) return null;
  const [payload, signature] = token.split('.');
  const expected = base64url(crypto.createHmac('sha256', secret).update(payload).digest());
  if (signature.length !== expected.length || !crypto.timingSafeEqual(Buffer.from(signature), Buffer.from(expected))) {
    return null;
  }
  try {
    const normalized = payload.replace(/-/g, '+').replace(/_/g, '/');
    const data = JSON.parse(Buffer.from(normalized, 'base64').toString('utf8'));
    return data.exp > Date.now() ? data : null;
  } catch (_) {
    return null;
  }
}

function cookies(req) {
  return Object.fromEntries(String(req.headers.cookie || '').split(';').map((part) => {
    const index = part.indexOf('=');
    return index < 0 ? ['', ''] : [part.slice(0, index).trim(), decodeURIComponent(part.slice(index + 1))];
  }).filter(([key]) => key));
}

function requireUser(req, secret, writeAccess) {
  const user = parseSession(cookies(req).session, secret);
  if (!user) throw new HttpError(401, 'Требуется аутентификация');
  if (writeAccess && user.role !== 'admin') throw new HttpError(403, 'Недостаточно прав');
  return user;
}

function readJson(req) {
  return new Promise((resolve, reject) => {
    if (!String(req.headers['content-type'] || '').toLowerCase().startsWith('application/json')) {
      reject(new HttpError(400, 'Content-Type должен быть application/json'));
      return;
    }
    let body = '';
    req.setEncoding('utf8');
    req.on('data', (chunk) => {
      body += chunk;
      if (Buffer.byteLength(body) > MAX_BODY_SIZE) {
        reject(new HttpError(400, 'Слишком большой запрос'));
        req.destroy();
      }
    });
    req.on('end', () => {
      try { resolve(JSON.parse(body || '{}')); }
      catch (_) { reject(new HttpError(400, 'Некорректный JSON')); }
    });
    req.on('error', reject);
  });
}

function sendJson(res, status, data, extraHeaders) {
  const body = status === 204 ? '' : JSON.stringify(data);
  res.writeHead(status, {
    'Content-Type': 'application/json; charset=utf-8',
    'Content-Length': Buffer.byteLength(body),
    'X-Content-Type-Options': 'nosniff',
    ...extraHeaders
  });
  res.end(body);
}

function etagFor(value) {
  return '"' + crypto.createHash('sha256').update(JSON.stringify(value)).digest('hex') + '"';
}

function serveStatic(req, res, pathname) {
  const requested = pathname === '/' || pathname === '/swagger' ? 'index.html' : pathname.slice(1);
  const resolved = path.resolve(PUBLIC_DIR, requested);
  if (!resolved.startsWith(PUBLIC_DIR + path.sep)) throw new HttpError(403, 'Доступ запрещён');
  let data;
  try { data = fs.readFileSync(resolved); }
  catch (_) { throw new HttpError(404, 'Файл не найден'); }
  const type = path.extname(resolved) === '.css' ? 'text/css; charset=utf-8'
    : path.extname(resolved) === '.js' ? 'application/javascript; charset=utf-8'
      : 'text/html; charset=utf-8';
  res.writeHead(200, { 'Content-Type': type, 'Content-Length': data.length });
  res.end(data);
}

function createRequestHandler(options) {
  const store = new AlbumStore(options.dataFile || DEFAULT_DATA_FILE);
  const secret = options.secret || process.env.SESSION_SECRET || 'lab2-local-development-secret';
  const secureCookie = options.secureCookie !== false;
  const users = {
    demo: { username: 'demo', password: 'demo', role: 'admin' },
    viewer: { username: 'viewer', password: 'viewer', role: 'viewer' }
  };

  return async function handle(req, res) {
    try {
      const url = new URL(req.url, 'http://' + (req.headers.host || 'localhost'));
      const pathname = url.pathname;
      const accept = String(req.headers.accept || '*/*');
      if (pathname.startsWith('/api/') && !accept.includes('application/json') && !accept.includes('*/*')) {
        throw new HttpError(400, 'Accept должен разрешать application/json');
      }
      console.log(`[${new Date().toISOString()}] ${req.method} ${pathname} host=${req.headers.host || '-'} agent=${req.headers['user-agent'] || '-'}`);

      if (req.method === 'OPTIONS') {
        res.writeHead(204, {
          'Access-Control-Allow-Origin': '*',
          'Access-Control-Allow-Headers': 'Content-Type, Accept',
          'Access-Control-Allow-Methods': 'GET, POST, PUT, PATCH, DELETE, OPTIONS'
        });
        return res.end();
      }
      if (pathname === '/openapi.json' && req.method === 'GET') {
        const data = fs.readFileSync(OPENAPI_FILE);
        res.writeHead(200, { 'Content-Type': 'application/json; charset=utf-8', 'Content-Length': data.length });
        return res.end(data);
      }
      if (!pathname.startsWith('/api/')) return serveStatic(req, res, pathname);

      if (pathname === '/api/v1/health' && req.method === 'GET') {
        return sendJson(res, 200, {
          status: 'ok',
          instance: process.env.INSTANCE_ID || os.hostname(),
          version: process.env.APP_VERSION || 'blue-1.0.0',
          time: new Date().toISOString()
        }, { 'Cache-Control': 'no-store' });
      }
      if (pathname === '/api/v1/auth/login' && req.method === 'POST') {
        const body = await readJson(req);
        const candidate = users[String(body.username || '')];
        if (!candidate || candidate.password !== String(body.password || '')) {
          throw new HttpError(401, 'Неверное имя пользователя или пароль');
        }
        const session = issueSession(candidate, secret);
        return sendJson(res, 200, { username: candidate.username, role: candidate.role }, {
          'Set-Cookie': `session=${encodeURIComponent(session)}; Path=/; HttpOnly; SameSite=Strict${secureCookie ? '; Secure' : ''}`,
          'Cache-Control': 'no-store'
        });
      }
      if (pathname === '/api/v1/auth/logout' && req.method === 'POST') {
        return sendJson(res, 204, null, {
          'Set-Cookie': `session=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict${secureCookie ? '; Secure' : ''}`
        });
      }
      if (options.testMode && pathname === '/api/v1/debug/error') {
        throw new Error('Test error');
      }

      const albumMatch = pathname.match(/^\/api\/v1\/albums(?:\/(\d+))?$/);
      if (!albumMatch) throw new HttpError(404, 'Маршрут не найден');
      const id = albumMatch[1] ? Number(albumMatch[1]) : null;

      if (req.method === 'GET') {
        requireUser(req, secret, false);
        const payload = id ? store.find(id) : store.list();
        if (!payload) throw new HttpError(404, 'Альбом не найден');
        const tag = etagFor(payload);
        const headers = {
          ETag: tag,
          'Cache-Control': 'private, max-age=30, must-revalidate',
          'Last-Modified': store.lastModified.toUTCString()
        };
        if (req.headers['if-none-match'] === tag) {
          res.writeHead(304, headers);
          return res.end();
        }
        return sendJson(res, 200, payload, headers);
      }
      requireUser(req, secret, true);
      if (req.method === 'POST' && id === null) {
        const album = store.create(await readJson(req));
        return sendJson(res, 201, album, { Location: `/api/v1/albums/${album.id}` });
      }
      if ((req.method === 'PUT' || req.method === 'PATCH') && id !== null) {
        return sendJson(res, 200, store.update(id, await readJson(req), req.method === 'PATCH'));
      }
      if (req.method === 'DELETE' && id !== null) {
        store.remove(id);
        return sendJson(res, 204, null);
      }
      throw new HttpError(404, 'Маршрут не найден');
    } catch (error) {
      const status = error instanceof HttpError ? error.status : 500;
      if (status === 500) console.error(error.stack || error);
      if (!res.headersSent) sendJson(res, status, { error: status === 500 ? 'Внутренняя ошибка сервера' : error.message }, {
        'Cache-Control': 'no-store'
      });
    }
  };
}

function createServer(options = {}) {
  const handler = createRequestHandler(options);
  if (options.https === false) return http.createServer(handler);
  const keyFile = options.keyFile || process.env.TLS_KEY || path.join(__dirname, 'certs', 'dev-key.pem');
  const certFile = options.certFile || process.env.TLS_CERT || path.join(__dirname, 'certs', 'dev-cert.pem');
  if (!fs.existsSync(keyFile) || !fs.existsSync(certFile)) {
    throw new Error('Нет HTTPS-сертификата. Выполните scripts/generate-cert.ps1 или задайте TLS_KEY и TLS_CERT.');
  }
  return https.createServer({ key: fs.readFileSync(keyFile), cert: fs.readFileSync(certFile) }, handler);
}

if (require.main === module) {
  const useHttps = process.env.HTTPS !== '0';
  const port = Number(process.env.PORT) || (useHttps ? 8443 : 8082);
  const host = process.env.HOST || '0.0.0.0';
  const server = createServer({ https: useHttps });
  server.listen(port, host, () => {
    const scheme = useHttps ? 'https' : 'http';
    console.log(`Music API: ${scheme}://localhost:${port}/swagger`);
    Object.values(os.networkInterfaces()).flat().forEach((net) => {
      if (net && net.family === 'IPv4' && !net.internal) {
        console.log(`Aurora OS: ${scheme}://${net.address}:${port}/swagger`);
      }
    });
  });
}

module.exports = { createServer, AlbumStore, normalizeAlbum };
