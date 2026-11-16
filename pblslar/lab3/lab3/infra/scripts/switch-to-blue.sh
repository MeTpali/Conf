#!/usr/bin/env bash
# Откат blue-green: возвращает трафик на blue.
set -euo pipefail
INFRA="$(cd "$(dirname "$0")/.." && pwd)"
cat "$INFRA/upstream-blue.conf" > "$INFRA/active-upstream.conf"
docker compose -f "$INFRA/docker-compose.yml" exec -T nginx nginx -t
docker compose -f "$INFRA/docker-compose.yml" exec -T nginx nginx -s reload
echo 'Трафик возвращён на blue.'
