#!/usr/bin/env bash
# Опрашивает балансировщик и показывает, какой экземпляр ответил.
# Запустить в отдельном терминале и остановить один backend:
#   docker compose stop blue1
set -u
URL="${1:-http://localhost:8080}/api/v1/health"
COUNT="${COUNT:-20}"
ok=0; fail=0
for i in $(seq 1 "$COUNT"); do
  if body=$(curl -fsS --max-time 3 "$URL"); then
    ok=$((ok + 1))
    echo "$i: $(echo "$body" | sed -E 's/.*"instance":"([^"]*)".*"version":"([^"]*)".*/\1 \2/')"
  else
    fail=$((fail + 1))
    echo "$i: ОШИБКА"
  fi
  sleep 0.5
done
echo "Успешно: $ok, ошибок: $fail"
[ "$fail" -eq 0 ]
