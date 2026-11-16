#!/usr/bin/env bash
# Поднимает Debian с systemd, включает два экземпляра API через шаблон и проверяет
# автозапуск, ответы и перезапуск после падения (Restart=always).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
NAME=aurora-systemd-test
docker build -q -f "$ROOT/lab3/infra/systemd/Dockerfile.test" -t $NAME "$ROOT" >/dev/null
docker rm -f $NAME >/dev/null 2>&1 || true
docker run -d --name $NAME --privileged --cgroupns=host -v /sys/fs/cgroup:/sys/fs/cgroup:rw $NAME >/dev/null
trap 'docker rm -f $NAME >/dev/null' EXIT
for _ in $(seq 1 30); do
  docker exec $NAME systemctl is-active aurora-music-api@1 aurora-music-api@2 >/dev/null 2>&1 && break
  sleep 1
done
run() { echo "\$ $*"; docker exec $NAME "$@"; echo; }
run systemctl list-units 'aurora-music-api@*' --no-pager --no-legend
run systemctl is-enabled aurora-music-api@1 aurora-music-api@2
run curl -s http://127.0.0.1:8082/api/v1/health
run curl -s http://127.0.0.1:8083/api/v1/health
PID=$(docker exec $NAME systemctl show -p MainPID --value aurora-music-api@1)
echo "\$ kill -9 $PID   # аварийное завершение экземпляра 1"
docker exec $NAME sh -c "kill -9 $PID"
sleep 4
NEW=$(docker exec $NAME systemctl show -p MainPID --value aurora-music-api@1)
echo "PID до: $PID, после: $NEW"
echo
run systemctl show -p NRestarts aurora-music-api@1
run curl -s http://127.0.0.1:8082/api/v1/health
run journalctl -u aurora-music-api@1 --no-pager -n 6 -o cat
