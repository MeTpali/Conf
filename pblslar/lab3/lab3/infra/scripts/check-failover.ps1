$ErrorActionPreference = 'Stop'
1..10 | ForEach-Object {
    $health = Invoke-RestMethod -Uri 'http://localhost:8080/api/v1/health' -TimeoutSec 3
    "{0}: {1} {2}" -f $_, $health.instance, $health.version
    Start-Sleep -Milliseconds 500
}
