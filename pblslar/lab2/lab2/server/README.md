# Сервер ЛР2

HTTPS REST API музыкальной коллекции без сторонних npm-зависимостей.

1. Сертификат уже лежит в `certs/`. Пересоздать его:
   `./scripts/generate-cert.sh` (macOS / Linux) или
   `powershell -ExecutionPolicy Bypass -File scripts/generate-cert.ps1`
   (Windows).
2. Запустить: `node server.js`. Без HTTPS: `HTTPS=0 node server.js`, тогда
   порт 8082.
3. Открыть `https://localhost:8443/swagger` и принять локальный сертификат.

Учётные записи: `demo/demo` — полный CRUD, `viewer/viewer` — только чтение.
Автотест: `node test-server.js`.
