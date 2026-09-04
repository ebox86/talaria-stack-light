<#
.SYNOPSIS
    Smoke-tests a running Talaria Stack Light device over HTTP.

.DESCRIPTION
    Exercises every API endpoint against a real device: health, status
    read, each status-changing endpoint, and the light bench self-test.
    Intended for quick regression checks after a firmware change or a
    rewiring, without needing curl or Postman.

.PARAMETER DeviceHost
    Hostname or IP of the device. Defaults to the mDNS name the firmware
    advertises ("talaria-stack-01.local"). Override with the DHCP IP
    (e.g. 192.168.1.183) if mDNS isn't resolving on your network.

.PARAMETER ApiKey
    The API_KEY value from include/secrets.h. Every POST/DELETE check
    below needs this or comes back 401.

.EXAMPLE
    ./scripts/smoke_test.ps1 -ApiKey change-me
    ./scripts/smoke_test.ps1 -DeviceHost 192.168.1.183 -ApiKey change-me
#>

param(
    [string]$DeviceHost = "talaria-stack-01.local",
    [int]$Port = 80,
    [Parameter(Mandatory = $true)]
    [string]$ApiKey
)

$base = "http://${DeviceHost}:${Port}"

function Invoke-Check {
    param(
        [string]$Name,
        [string]$Method,
        [string]$Path,
        [hashtable]$Body = $null,
        [switch]$RequiresAuth
    )

    $uri = "$base$Path"
    $headers = @{}
    if ($RequiresAuth) {
        $headers["X-Api-Key"] = $ApiKey
    }

    try {
        if ($Body) {
            $json = $Body | ConvertTo-Json -Compress
            $response = Invoke-RestMethod -Uri $uri -Method $Method -Headers $headers -Body $json -ContentType "application/json" -TimeoutSec 5
        } else {
            $response = Invoke-RestMethod -Uri $uri -Method $Method -Headers $headers -TimeoutSec 5
        }
        Write-Host "[PASS] $Name ($Method $Path)" -ForegroundColor Green
        Write-Host "       $($response | ConvertTo-Json -Compress)"
        return $true
    } catch {
        Write-Host "[FAIL] $Name ($Method $Path): $($_.Exception.Message)" -ForegroundColor Red
        return $false
    }
}

Write-Host "Talaria Stack Light smoke test -> $base"
Write-Host "========================================"

$results = @(
    Invoke-Check -Name "Health"          -Method GET  -Path "/health"
    Invoke-Check -Name "Get status"      -Method GET  -Path "/status"
    Invoke-Check -Name "Set open"        -Method POST -Path "/status/open"     -RequiresAuth
    Invoke-Check -Name "Set warning"     -Method POST -Path "/status/warning"  -RequiresAuth
    Invoke-Check -Name "Set critical"    -Method POST -Path "/status/critical" -RequiresAuth
    Invoke-Check -Name "Set closed"      -Method POST -Path "/status/closed"   -RequiresAuth
    Invoke-Check -Name "Light self-test" -Method POST -Path "/test/cycle"      -RequiresAuth
    Invoke-Check -Name "Hold all lights on" -Method POST -Path "/test/all-on"  -RequiresAuth
    Invoke-Check -Name "All lights off"     -Method POST -Path "/test/all-off" -RequiresAuth
    Invoke-Check -Name "Post signal" -Method POST -Path "/api/v1/signals" -RequiresAuth -Body @{
        source = "smoke-test"; condition = "CHECK"; severity = "warning"; message = "smoke test"
    }
    Invoke-Check -Name "List signals"  -Method GET    -Path "/api/v1/signals"
    Invoke-Check -Name "Clear signal"  -Method DELETE -Path "/api/v1/signals/smoke-test/CHECK" -RequiresAuth
    Invoke-Check -Name "Heartbeat"     -Method POST   -Path "/heartbeat" -RequiresAuth
)

$failCount = ($results | Where-Object { -not $_ }).Count

Write-Host "========================================"
if ($failCount -eq 0) {
    Write-Host "All checks passed." -ForegroundColor Green
    exit 0
} else {
    Write-Host "$failCount check(s) failed." -ForegroundColor Red
    exit 1
}
