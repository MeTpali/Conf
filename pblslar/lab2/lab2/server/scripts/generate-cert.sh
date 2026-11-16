#!/usr/bin/env bash
# Самоподписанный сертификат для HTTPS-сервера ЛР2 (macOS / Linux).
set -euo pipefail
CERTS="$(cd "$(dirname "$0")/.." && pwd)/certs"
mkdir -p "$CERTS"
openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout "$CERTS/dev-key.pem" -out "$CERTS/dev-cert.pem" -days 365 \
  -subj '/CN=aurora-lab2.local' -addext 'subjectAltName=DNS:localhost,IP:127.0.0.1'
echo "Сертификат создан в $CERTS"
