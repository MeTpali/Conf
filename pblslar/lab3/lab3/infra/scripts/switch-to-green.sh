#!/usr/bin/env bash
# Blue-green: поднимает green, проверяет health и переключает nginx без остановки.
set -euo pipefail
INFRA="$(cd "$(dirname "$0")/.." && pwd)"
COMPOSE=(docker compose -f "$INFRA/docker-compose.yml" --profile green)
"${COMPOSE[@]}" up -d --build green1 green2
for name in green1 green2; do
  healthy=''
  for _ in $(seq 1 20); do
    # 127.0.0.1, а не localhost: busybox wget выбирает IPv6, а Node слушает IPv4.
    healthy=$("${COMPOSE[@]}" exec -T "$name" wget -qO- http://127.0.0.1:8082/api/v1/health 2>/dev/null) && break
    sleep 1
  done
  [ -n "$healthy" ] || { echo "$name не прошёл health-check, переключение отменено" >&2; exit 1; }
  echo "$name: $healthy"
done
# cat > сохраняет inode файла, смонтированного в контейнер nginx.
cat "$INFRA/upstream-green.conf" > "$INFRA/active-upstream.conf"
"${COMPOSE[@]}" exec -T nginx nginx -t
"${COMPOSE[@]}" exec -T nginx nginx -s reload
echo 'Трафик переключён на green. Blue оставлен для отката: scripts/switch-to-blue.sh'
