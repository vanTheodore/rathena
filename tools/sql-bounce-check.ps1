# sql-bounce-check: regression for runtime SQL reconnects (Sql_P_Reconnect).
# Procedure: start the DB, boot the three servers, prove green with
# proto-smoke, BOUNCE the database (kills every server SQL handle at once,
# no timing games), then run smoke twice: the first post-bounce run MUST fail
# (fail-closed: the statements caught by the loss return errors, they are
# never re-executed), and the follow-up run MUST be fully green (the
# re-established connection serves normally). A general_log scan after each
# smoke run asserts no write statement reached the server twice (the
# duplicate-execution hazard): any repeated write fails the run. With -DbDown the bounce is replaced by an outage: the DB
# stays down for one smoke run (expected red, orderly behavior, no crash),
# then comes back for a recovery run (expected green, no corruption).
# Exit 0 + FAILURES=0 on success; nonzero otherwise (ABORT lines mean
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
#     -DbUser root -DbPass <db-pass> -User <test-user> -Pass <test-pass> -Char <test-char> [-DbDown]
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
  [int]$BootWait = 75,
  [switch]$DbDown
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
    if ($LASTEXITCODE -eq 0) { $script:note = "DB-READY $tag"; return $true }
  }
  $script:note = "ABORT-NODB $tag"; $script:fails++; return $false
}
# Proves the bounce really happened: a surviving instance keeps its uptime.
function CheckFresh {
  $row = (& $M @(DbArgs) -N -e "SHOW GLOBAL STATUS LIKE 'Uptime';" 2>$null | Select-Object -Last 1)
  $u = ("$row" -split '\s+')[-1]
  if ("$u" -match '^\d+$' -and [int]"$u" -lt 180) { $script:note = "BOUNCE-OK uptime=$u"; return $true }
  $script:note = "ABORT-BOUNCE-FAILED uptime=$u"; $script:fails++; return $false
}
function LogOn {
  & $M @(DbArgs) -N -e "SET GLOBAL log_output='TABLE';" > $null 2>&1
  & $M @(DbArgs) -N -e "SET GLOBAL general_log=1;" > $null 2>&1
  $v = (& $M @(DbArgs) -N -e "SELECT @@general_log;" 2>$null | Select-Object -Last 1)
  if ("$v".Trim() -eq "1") { $script:note = "GENERAL-LOG on"; return $true }
  $script:note = "ABORT-LOGON"; $script:fails++; return $false
}
function LogOff { & $M @(DbArgs) -N -e "SET GLOBAL general_log=0;" > $null 2>&1 }
function LogClear { & $M @(DbArgs) -N -e "TRUNCATE TABLE mysql.general_log;" > $null 2>&1 }
# No write statement may reach the server twice within one phase (only
# INSERT/UPDATE/DELETE/REPLACE rows are checked; reads and pings are out
# of scope for the duplicate-execution hazard).
function LogScan($tag) {
  $out = & $M @(DbArgs) -N --batch -e "SELECT argument, COUNT(*) FROM mysql.general_log WHERE command_type='Query' AND UPPER(LEFT(TRIM(argument),6)) IN ('INSERT','UPDATE','DELETE','REPLAC') GROUP BY argument HAVING COUNT(*) > 1;" 2>&1
  if ($LASTEXITCODE -ne 0) { $script:note = "ABORT-LOGSCAN $tag"; $script:fails++; return }
  $rows = @($out | Where-Object { "$_".Trim() -ne "" })
  if ($rows.Count -eq 0) { $script:note = "LOGSCAN-$tag-CLEAN (no write ran twice)"; return }
  $script:note = "DUP-WRITE $tag:"
  $rows | ForEach-Object { $script:note += "`n  $_" }
  $script:fails += $rows.Count
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
function StartDb { Start-Process -FilePath $D -ArgumentList "--datadir=$DataDir", "--bind-address=127.0.0.1", "--port=$DbPort" -WorkingDirectory $MariadbBin -NoNewWindow }
function RunSmoke($tag) {
  powershell -ExecutionPolicy Bypass -File $Smoke -User $User -Pass $Pass -Char $Char -LoginPort $LoginPort
  $c = $LASTEXITCODE
  "SMOKE-$tag-EXIT=$c"
  $script:fails += $c
}
StopServers; StopDb; Start-Sleep -Seconds 4
StartDb
$ok = WaitDb "pre"; "$script:note"
if (!$ok) { StopDb; "FAILURES=$script:fails"; exit $script:fails }
BootServers
$ok = LogOn; "$script:note"
if (!$ok) { StopServers; StopDb; "FAILURES=$script:fails"; exit $script:fails }
LogClear; RunSmoke "pre"; LogScan "pre"; "$script:note"
if ($DbDown) {
  StopDb; Start-Sleep -Seconds 8
  powershell -ExecutionPolicy Bypass -File $Smoke -User $User -Pass $Pass -Char $Char -LoginPort $LoginPort > $null 2>&1
  $c = $LASTEXITCODE
  "SMOKE-down-EXIT=$c (expected non-zero)"
  if ($c -eq 0) { "UNEXPECTED-GREEN-DOWN"; $script:fails++ }
  StartDb
  $ok = WaitDb "recovery"; "$script:note"
  if (!$ok) { StopServers; StopDb; "FAILURES=$script:fails"; exit $script:fails }
  StopServers; BootServers
  LogClear; RunSmoke "recovered"; LogScan "recovered"; "$script:note"
} else {
  StopDb; Start-Sleep -Seconds 8
  StartDb
  $ok = WaitDb "post"; "$script:note"
  if (!$ok) { StopServers; StopDb; "FAILURES=$script:fails"; exit $script:fails }
  $ok = CheckFresh; "$script:note"
  if (!$ok) { StopServers; StopDb; "FAILURES=$script:fails"; exit $script:fails }
  LogClear
  powershell -ExecutionPolicy Bypass -File $Smoke -User $User -Pass $Pass -Char $Char -LoginPort $LoginPort
  $cPost = $LASTEXITCODE
  "SMOKE-post-EXIT=$cPost (expected non-zero: first touch after loss fails clean)"
  if ($cPost -eq 0) { "UNEXPECTED-GREEN-POST"; $script:fails++ }
  LogScan "post"; "$script:note"
  RunSmoke "healed"
  LogScan "healed"; "$script:note"
}
LogOff; StopServers; StopDb
"FAILURES=$script:fails"
exit $script:fails
