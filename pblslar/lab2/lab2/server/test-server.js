'use strict';

const assert = require('assert');
const fs = require('fs');
const http = require('http');
const os = require('os');
const path = require('path');
const { createServer } = require('./server');

function request(port, method, pathname, body, cookie, headers = {}) {
  return new Promise((resolve, reject) => {
    const payload = body === undefined ? null : JSON.stringify(body);
    const req = http.request({
      host: '127.0.0.1', port, method, path: pathname,
      headers: {
        Accept: 'application/json',
        ...(payload ? { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(payload) } : {}),
        ...(cookie ? { Cookie: cookie } : {}),
        ...headers
      }
    }, (res) => {
      let text = '';
      res.setEncoding('utf8');
      res.on('data', (chunk) => { text += chunk; });
      res.on('end', () => resolve({
        status: res.statusCode,
        headers: res.headers,
        body: text ? JSON.parse(text) : null
      }));
    });
    req.on('error', reject);
    if (payload) req.write(payload);
    req.end();
  });
}

async function login(port, username, password) {
  const response = await request(port, 'POST', '/api/v1/auth/login', { username, password });
  assert.strictEqual(response.status, 200);
  return response.headers['set-cookie'][0].split(';')[0];
}

async function run() {
  const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'aurora-lab2-'));
  const server = createServer({
    https: false,
    secureCookie: false,
    testMode: true,
    dataFile: path.join(temp, 'albums.json'),
    secret: 'test-secret'
  });
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  const port = server.address().port;
  try {
    assert.strictEqual((await request(port, 'GET', '/api/v1/albums')).status, 401);
    const admin = await login(port, 'demo', 'demo');
    const viewer = await login(port, 'viewer', 'viewer');

    const initial = await request(port, 'GET', '/api/v1/albums', undefined, admin);
    assert.strictEqual(initial.status, 200);
    assert.ok(initial.headers.etag);
    assert.strictEqual((await request(port, 'GET', '/api/v1/albums', undefined, admin, {
      'If-None-Match': initial.headers.etag
    })).status, 304);

    const created = await request(port, 'POST', '/api/v1/albums', {
      performer: 'Молчат Дома', album: 'Этажи', year: 2018, genre: 'Post-punk'
    }, admin);
    assert.strictEqual(created.status, 201);
    const id = created.body.id;

    assert.strictEqual((await request(port, 'PATCH', `/api/v1/albums/${id}`, {
      genre: 'Post-punk / New wave'
    }, admin)).status, 200);
    assert.strictEqual((await request(port, 'DELETE', `/api/v1/albums/${id}`, undefined, viewer)).status, 403);
    assert.strictEqual((await request(port, 'DELETE', `/api/v1/albums/${id}`, undefined, admin)).status, 204);
    assert.strictEqual((await request(port, 'GET', `/api/v1/albums/${id}`, undefined, admin)).status, 404);
    assert.strictEqual((await request(port, 'GET', '/api/v1/debug/error', undefined, admin)).status, 500);

    // Второй экземпляр с тем же файлом данных (как за балансировщиком в ЛР3).
    const second = createServer({
      https: false, secureCookie: false, dataFile: path.join(temp, 'albums.json'), secret: 'test-secret'
    });
    await new Promise((resolve) => second.listen(0, '127.0.0.1', resolve));
    try {
      const shared = await request(port, 'POST', '/api/v1/albums', {
        performer: 'Сплин', album: 'Гранатовый альбом', year: 1998, genre: 'Рок'
      }, admin);
      const seen = await request(second.address().port, 'GET', `/api/v1/albums/${shared.body.id}`, undefined, admin);
      assert.strictEqual(seen.status, 200);
      const next = await request(second.address().port, 'POST', '/api/v1/albums', {
        performer: 'Аквариум', album: 'Радио Африка', year: 1983, genre: 'Рок'
      }, admin);
      assert.notStrictEqual(next.body.id, shared.body.id);
      assert.strictEqual((await request(port, 'GET', '/api/v1/albums', undefined, admin)).body.length, 4);
    } finally {
      await new Promise((resolve) => second.close(resolve));
    }
    console.log('OK: auth, CRUD, 200/201/204/400/401/403/404/500 and ETag and shared storage work');
  } finally {
    await new Promise((resolve) => server.close(resolve));
    fs.rmSync(temp, { recursive: true, force: true });
  }
}

run().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
