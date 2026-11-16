# Лабораторная работа 3

ЛР3 продолжает музыкальную коллекцию из ЛР2 (вариант 9).

- `backend` — API из ЛР2 плюс WebSocket push (`/ws/notifications`),
  заголовок `X-Instance-Id` и рассылка push при изменении коллекции.
  Экземпляры работают с общим файлом данных и перечитывают его при изменении,
  поэтому за балансировщиком видят одни и те же данные.
- `infra` — Docker Compose (blue1, blue2, green1, green2, nginx), nginx как
  reverse proxy и балансировщик (`least_conn`, `proxy_next_upstream`),
  systemd-шаблон `aurora-music-api@.service`, скрипты проверки и blue-green.
- `aurora-client` — CMake-проект Qt/QML/C++: health-check, загрузка альбомов
  через балансировщик, повторы с экспоненциальной задержкой, push по WebSocket.

## Часть 1. Инфраструктура

macOS / Linux:

```bash
cd lab3/infra
docker compose up -d --build
./scripts/check-failover.sh            # 20 запросов, видно какой экземпляр ответил
```

Windows: `docker compose up -d --build`, затем
`powershell -File scripts/check-failover.ps1`.

Push-уведомления в терминале: `node scripts/watch-push.js`.

### Отказоустойчивость

В одном терминале запустить `COUNT=40 ./scripts/check-failover.sh`, в другом —
`docker compose kill blue1`. Все запросы продолжают обслуживаться `blue-2`,
ошибок нет. Вернуть: `docker compose start blue1`.

### Blue-green деплой

```bash
COUNT=60 ./scripts/check-failover.sh   # в отдельном терминале
./scripts/switch-to-green.sh           # поднимает green, проверяет health, reload nginx
docker compose stop blue1 blue2        # после проверки green
```

Ответы меняются с `blue-1.0.0` на `green-1.1.0` без единой ошибки.
Откат: `docker compose start blue1 blue2 && ./scripts/switch-to-blue.sh`.
Скрипт переключения отказывается менять upstream, если green не прошёл
health-check.

### systemd вместо Docker

```bash
sudo cp infra/systemd/aurora-music-api@.service /etc/systemd/system/
# /etc/aurora-music/1.env: INSTANCE_ID=node-1  PORT=8082  DATA_FILE=/var/lib/aurora-music/albums.json
# /etc/aurora-music/2.env: INSTANCE_ID=node-2  PORT=8083  DATA_FILE=/var/lib/aurora-music/albums.json
sudo systemctl daemon-reload
sudo systemctl enable --now aurora-music-api@1 aurora-music-api@2
```

В upstream nginx тогда указываются `127.0.0.1:8082` и `127.0.0.1:8083`.

Проверка шаблона в Linux с настоящим systemd (Debian в Docker): поднимаются
`aurora-music-api@1` и `@2` с файлами окружения `systemd/1.env` и `2.env`,
проверяются автозапуск, ответы и перезапуск после `kill -9`:

```bash
./infra/systemd/test-systemd.sh
```

## Часть 2. Клиент Aurora OS

Открыть `aurora-client/CMakeLists.txt` в Aurora IDE и собрать. Адреса по
умолчанию для эмулятора: `http://10.0.2.2:8080` и
`ws://10.0.2.2:8080/ws/notifications`. Для телефона указать IP компьютера в
локальной сети.

Сборка, подпись и установка на телефон:

```bash
sfdk config target=AuroraOS-5.2.1.200-armv7hl   # или aarch64 — по архитектуре телефона
sfdk build lab3/aurora-client                    # rpmbuild по rpm/ru.mai.resilient_music.spec
sfdk engine exec sudo rpmsign-external sign \
    --key regular_key.pem --cert regular_cert.pem RPMS/ru.mai.resilient_music-*.rpm
sfdk deploy --sdk                                 # или скопировать RPM и установить на телефоне
```

`regular_key.pem` и `regular_cert.pem` — ключ и сертификат разработчика с
developer.auroraos.ru. В некоторых версиях SDK та же операция вызывается как
`sign_package` (так она названа в задании); точное имя команды стоит сверить
с документацией установленной версии SDK. В Aurora IDE то же самое делается в настройках подписи
пакетов (Aurora OS → «Подпись пакетов»).

Что показать на телефоне:

1. Экран показывает экземпляр и версию backend, список альбомов и экземпляр,
   который его отдал.
2. Пункт меню «Добавить тестовый альбом» → приходит push «Коллекция
   обновлена», список обновляется.
3. `docker compose kill blue1` — приложение продолжает работать через `blue-2`.
   Если push был подключён к убитому экземпляру, клиент сам переподключается.
4. Blue-green: версия на экране меняется на `green-1.1.0`, после остановки blue
   push переподключается к green.
5. Остановить весь стенд — растёт счётчик повторов, последний список альбомов
   остаётся на экране, после запуска всё восстанавливается.
