# sql-bounce-check: regression for runtime SQL reconnects (Sql_P_Reconnect).
# Procedure: start the DB, boot the three servers, prove green with
# proto-smoke, BOUNCE the database (kills every server SQL handle at once,
# no timing games), prove green again. The post-bounce run is THE assertion:
# every handle must reconnect with intact parameters and keep serving.
# Exit 0 + FAILURES=0 twice on success; nonzero otherwise (ABORT lines mean
# the harness itself failed, not the servers).
#
# Everything environment-specific is a parameter on purpose: no hosts, paths
# or secrets live in this file. -DbPass/-User/-Pass are mandatory precisely
# so nothing credential-like is embedded. Point the servers at a disposable
# database; never at production data.
#
# Prerequisite: the servers must run with mysql_reconnect_type: 1 in their
# SQL config, otherwise there is no statement-level reconnect to exercise
# (type 2 fails closed: the post-bounce run fails with or without any fix).
#
# Usage (PowerShell):
#   powershell -ExecutionPolicy Bypass -File tools/sql-bounce-check.ps1 `
#     -MariadbBin C:\path\to\mariadb\bin -DataDir C:\path\to\data `
#     -ServerDir C:\path\to\rathena -MsysBin C:\msys64\ucrt64\bin `
#     -DbUser root -DbPass <db-pass> -User <test-user> -Pass <test-pass> -Char <test-char>
param(
  [Parameter(Mandatory=$true)][string]$MariadbBin,
  [Parameter(Mandatory=$true)][string]$DataDir,
  [Parameter(Mandatory=$true)][string]$ServerDir,
  [Parameter(Mandatory=$true)][string]$MsysBin,
  [Parameter(Mandatory=$true)][string]$DbUser,
  [Parameter(Mandatory=$true)][string]$DbPass,
  [Parameter(Mandatory=$true)][string]$User,
  [Parameter(Mandatory=$true)][string]$Pass,
  [Parameter(Mandatory=$true)][string]$Char,
  [int]$DbPort = 3307,
  [int]$LoginPort = 6900,
  [int]$BootWait = 75
)
$script:fails = 0
$M = Join-Path $MariadbBin "mariadb.exe"
$D = Join-Path $MariadbBin "mariadbd.exe"
$Smoke = Join-Path $PSScriptRoot "proto-smoke.ps1"
function DbArgs { return @("--host=127.0.0.1", "--port=$DbPort", "--user=$DbUser", "--password=$DbPass", "--skip-ssl") }
function WaitDb($tag) {
  for ($i = 0; $i -lt 30; $i++) {
    Start-Sleep -Seconds 3
    & $M @(DbArgs) -N -e "SELECT 1;" > $null 2>&1
    if ($LASTEXITCODE -eq 0) { "DB-READY $tag"; return $true }
  }
  "ABORT-NODB $tag"; $script:fails++; return $false
}
# Proves the bounce really happened: a surviving instance keeps its uptime.
function CheckFresh {
  $row = (& $M @(DbArgs) -N -e "SHOW GLOBAL STATUS LIKE 'Uptime';" 2>$null | Select-Object -Last 1)
  $u = ("$row" -split '\s+')[-1]
  if ("$u" -match '^\d+$' -and [int]"$u" -lt 180) { "BOUNCE-OK uptime=$u"; return $true }
  "ABORT-BOUNCE-FAILED uptime=$u"; $script:fails++; return $false
}
function BootServers {
  $env:PATH = "$MsysBin;$env:PATH"
  Start-Process -FilePath (Join-Path $ServerDir "login-server.exe") -WorkingDirectory $ServerDir -NoNewWindow -RedirectStandardOutput (Join-Path $ServerDir "t-login.out.log") -RedirectStandardError (Join-Path $ServerDir "t-login.err.log")
  Start-Sleep -Seconds 8
  Start-Process -FilePath (Join-Path $ServerDir "char-server.exe") -WorkingDirectory $ServerDir -NoNewWindow -PassThru -RedirectStandardOutput (Join-Path $ServerDir "t-char.out.log") -RedirectStandardError (Join-Path $ServerDir "t-char.err.log") | Out-Null
  Start-Sleep -Seconds 12
  Start-Process -FilePath (Join-Path $ServerDir "map-server.exe") -WorkingDirectory $ServerDir -NoNewWindow -RedirectStandardOutput (Join-Path $ServerDir "t-map.out.log") -RedirectStandardError (Join-Path $ServerDir "t-map.err.log") | Out-Null
  Start-Sleep -Seconds $BootWait
}
function StopServers { Get-Process login-server, char-server, map-server -ErrorAction SilentlyContinue | Stop-Process -Force }
function StopDb { Get-Process mariadbd, mysqld -ErrorAction SilentlyContinue | Stop-Process -Force }
function RunSmoke($tag) {
  powershell -ExecutionPolicy Bypass -File $Smoke -User $User -Pass $Pass -Char $Char -LoginPort $LoginPort
  $c = $LASTEXITCODE
  "SMOKE-$tag-EXIT=$c"
  $script:fails += $c
}
StopServers; StopDb; Start-Sleep -Seconds 4
Start-Process -FilePath $D -ArgumentList "--datadir=$DataDir", "--bind-address=127.0.0.1", "--port=$DbPort" -WorkingDirectory $MariadbBin -NoNewWindow
if (!(WaitDb "pre")) { StopDb; "FAILURES=$script:fails"; exit $script:fails }
BootServers
RunSmoke "pre"
StopDb; Start-Sleep -Seconds 8
Start-Process -FilePath $D -ArgumentList "--datadir=$DataDir", "--bind-address=127.0.0.1", "--port=$DbPort" -WorkingDirectory $MariadbBin -NoNewWindow
if (!(WaitDb "post")) { StopServers; StopDb; "FAILURES=$script:fails"; exit $script:fails }
if (!(CheckFresh)) { StopServers; StopDb; "FAILURES=$script:fails"; exit $script:fails }
RunSmoke "post"
StopServers; StopDb
"FAILURES=$script:fails"
exit $script:fails
