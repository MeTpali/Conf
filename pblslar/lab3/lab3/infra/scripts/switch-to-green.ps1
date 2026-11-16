$ErrorActionPreference = 'Stop'
$infra = Split-Path $PSScriptRoot -Parent
docker compose -f (Join-Path $infra 'docker-compose.yml') --profile green up -d --build green1 green2
$green1 = docker compose -f (Join-Path $infra 'docker-compose.yml') exec -T green1 wget -qO- http://127.0.0.1:8082/api/v1/health
$green2 = docker compose -f (Join-Path $infra 'docker-compose.yml') exec -T green2 wget -qO- http://127.0.0.1:8082/api/v1/health
if (-not $green1 -or -not $green2) {
    throw 'Green instances did not pass the health check.'
}
Write-Host "Green instances are healthy: $green1 / $green2"
Copy-Item (Join-Path $infra 'upstream-green.conf') (Join-Path $infra 'active-upstream.conf') -Force
docker compose -f (Join-Path $infra 'docker-compose.yml') exec nginx nginx -s reload
Write-Host 'Traffic switched to green. Keep blue running until verification completes.'
