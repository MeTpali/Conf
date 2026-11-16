$ErrorActionPreference = 'Stop'
$certDir = Join-Path $PSScriptRoot '..\certs'
New-Item -ItemType Directory -Path $certDir -Force | Out-Null
$openssl = Get-Command openssl -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -First 1
if (-not $openssl) {
  $candidates = @(
    'C:\Program Files\Git\mingw64\bin\openssl.exe',
    'C:\Program Files\SmartGit\git\mingw64\bin\openssl.exe'
  )
  $openssl = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $openssl) { throw 'OpenSSL not found. Install Git for Windows or set TLS_KEY and TLS_CERT manually.' }
& $openssl req -x509 -newkey rsa:2048 -nodes `
  -keyout (Join-Path $certDir 'dev-key.pem') `
  -out (Join-Path $certDir 'dev-cert.pem') `
  -days 365 `
  -subj '/CN=aurora-lab2.local' `
  -addext 'subjectAltName=DNS:localhost,IP:127.0.0.1'
Write-Host "Certificate created in $certDir"
