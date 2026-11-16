$ErrorActionPreference = 'Stop'
$infra = Split-Path $PSScriptRoot -Parent
Copy-Item (Join-Path $infra 'upstream-blue.conf') (Join-Path $infra 'active-upstream.conf') -Force
docker compose -f (Join-Path $infra 'docker-compose.yml') exec nginx nginx -s reload
Write-Host 'Traffic switched back to blue.'
