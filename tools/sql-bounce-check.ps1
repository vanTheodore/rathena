# sql-bounce-check: regression for runtime SQL reconnects (Sql_P_Reconnect).
# Procedure: start the DB, boot the three servers, prove green with
# proto-smoke, BOUNCE the database (kills every server SQL handle at once,
# no timing games), then run smoke twice: the first post-bounce run MUST fail
# (fail-closed: the statements caught by the loss return errors, they are
# never re-executed), and the follow-up run MUST be fully green (the
# re-established connection serves normally). With -DbDown the bounce is
# replaced by an outage: the DB stays down for one smoke run (expected red,
# orderly behavior, no crash), then comes back for a recovery run.
# The no-re-execution oracle: every phase creates a character with a
# per-phase-unique name. The pre-loss creation must exist exactly once;
# post-loss creations must exist at most once (zero means the statement died
# before reaching the server, which is legal fail-closed; two or more would
# prove a duplicate execution). Created test rows are deleted at the end.
# Exit 0 + FAILURES=0 on success; nonzero otherwise (ABORT lines mean
# the harness itself failed, not the servers).
#
# Everything environment-specific is a parameter on purpose: no hosts, paths
# or secrets live in this file. -DbPass/-User/-Pass are mandatory precisely
# so nothing credential-like is embedded. -Char must be alphanumeric
# (it is interpolated into a cleanup script). Point the servers at a
# disposable database; never at production data. The account needs two
# free character slots (1 and 2; slot 0 typically holds the fixture char).
#
# Prerequisite: the servers must run with mysql_reconnect_type: 1 in their
# SQL config, otherwise there is no statement-level reconnect to exercise
# (type 2 fails closed: the post-bounce run fails with or without any fix).
#
# Usage (PowerShell):
#   powershell -ExecutionPolicy Bypass -File tools/sql-bounce-check.ps1 `
#     -MariadbBin C:\path\to\mariadb\bin -DataDir C:\path\to\data `
#     -ServerDir C:\path\to\rathena -MsysBin C:\msys64\ucrt64\bin `
#     -DbUser root -DbPass <db-pass> -CharDb <test-db> `
#     -User <test-user> -Pass <test-pass> -Char <test-char> [-DbDown]
param(
  [Parameter(Mandatory=$true)][string]$MariadbBin,
  [Parameter(Mandatory=$true)][string]$DataDir,
  [Parameter(Mandatory=$true)][string]$ServerDir,
  [Parameter(Mandatory=$true)][string]$MsysBin,
  [Parameter(Mandatory=$true)][string]$DbUser,
  [Parameter(Mandatory=$true)][string]$DbPass,
  [Parameter(Mandatory=$true)][string]$CharDb,
  [Parameter(Mandatory=$true)][string]$User,
  [Parameter(Mandatory=$true)][string]$Pass,
  [Parameter(Mandatory=$true)][string]$Char,
  [int]$DbPort = 3307,
  [int]$LoginPort = 6900,
  [int]$BootWait = 75,
  [switch]$DbDown
)
if ($Char -notmatch '^[A-Za-z0-9]+$' -or $Char.Length -gt 20) { "ABORT-BAD-CHAR (alphanumeric, max 20)"; exit 2 }
$script:fails = 0
$M = Join-Path $MariadbBin "mariadb.exe"
$D = Join-Path $MariadbBin "mariadbd.exe"
$Smoke = Join-Path $PSScriptRoot "proto-smoke.ps1"
$Names = @()
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
# Rows with these names must exist exactly $want times (want=1 pre-loss proof
# of execution, want=0..1 post-loss: 0 = died before send, >=2 = duplicate).
function AssertCreates($tag, $wantExact) {
  $lines = foreach ($n in $Names) { 'SELECT COUNT(*) FROM `' + $CharDb + '`.`char` WHERE name=''' + $n + ''';' }
  $lines | Out-File (Join-Path $ServerDir "t-assert.sql") -Encoding ascii
  $vals = @(Get-Content (Join-Path $ServerDir "t-assert.sql") | & $M @(DbArgs) -N --batch 2>&1 | Where-Object { "$_".Trim() -match '^\d+$' })
  Remove-Item (Join-Path $ServerDir "t-assert.sql") -ErrorAction SilentlyContinue
  for ($i = 0; $i -lt $Names.Count; $i++) {
    $c = if ($i -lt $vals.Count) { $vals[$i].Trim() } else { "?" }
    $ok = if ($wantExact) { $c -eq "1" } else { $c -eq "0" -or $c -eq "1" }
    if ($ok) { "CREATE-COUNT ${tag} $($Names[$i])=$c" } else { "DUP-OR-MISSING ${tag} $($Names[$i])=$c"; $script:fails++ }
  }
}
function CleanupCreates {
  $list = ($Names | ForEach-Object { "'$_'" }) -join ","
  $sql = 'DELETE FROM `' + $CharDb + '`.`char` WHERE name IN (' + $list + ');'
  $sql | Out-File (Join-Path $ServerDir "t-cleanup.sql") -Encoding ascii -NoNewline
  Get-Content (Join-Path $ServerDir "t-cleanup.sql") | & $M @(DbArgs) 2>&1 | Select-Object -First 2
  Remove-Item (Join-Path $ServerDir "t-cleanup.sql") -ErrorAction SilentlyContinue
  "CLEANUP-DONE"
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
function RunSmoke($tag, $name, $slot) {
  powershell -ExecutionPolicy Bypass -File $Smoke -User $User -Pass $Pass -Char $name -Slot $slot -LoginPort $LoginPort
  $c = $LASTEXITCODE
  "SMOKE-${tag}-EXIT=$c"
  [void]($script:fails += $c)
}
StopServers; StopDb; Start-Sleep -Seconds 4
StartDb
$ok = WaitDb "pre"; "$script:note"
if (!$ok) { StopDb; "FAILURES=$script:fails"; exit $script:fails }
BootServers
$Names = @("${Char}1")
RunSmoke "pre" "${Char}1" 1
AssertCreates "pre" $true
if ($DbDown) {
  StopDb; Start-Sleep -Seconds 8
  powershell -ExecutionPolicy Bypass -File $Smoke -User $User -Pass $Pass -Char $Char -Slot 0 -LoginPort $LoginPort > $null 2>&1
  $c = $LASTEXITCODE
  "SMOKE-down-EXIT=$c (expected non-zero)"
  if ($c -eq 0) { "UNEXPECTED-GREEN-DOWN"; $script:fails++ }
  StartDb
  $ok = WaitDb "recovery"; "$script:note"
  if (!$ok) { StopServers; StopDb; "FAILURES=$script:fails"; exit $script:fails }
  StopServers; BootServers
  $Names = @("${Char}1", "${Char}2")
  RunSmoke "recovered" "${Char}2" 2
  AssertCreates "recovered" $false
} else {
  StopDb; Start-Sleep -Seconds 8
  StartDb
  $ok = WaitDb "post"; "$script:note"
  if (!$ok) { StopServers; StopDb; "FAILURES=$script:fails"; exit $script:fails }
  $ok = CheckFresh; "$script:note"
  if (!$ok) { StopServers; StopDb; "FAILURES=$script:fails"; exit $script:fails }
  $Names = @("${Char}1", "${Char}2")
  powershell -ExecutionPolicy Bypass -File $Smoke -User $User -Pass $Pass -Char "${Char}2" -Slot 2 -LoginPort $LoginPort
  $cPost = $LASTEXITCODE
  "SMOKE-post-EXIT=$cPost (expected non-zero: first touch after loss fails clean)"
  if ($cPost -eq 0) { "UNEXPECTED-GREEN-POST"; $script:fails++ }
  $Names = @("${Char}1", "${Char}2")
  RunSmoke "healed" "${Char}2" 2
  AssertCreates "final" $false
}
CleanupCreates
StopServers; StopDb
"FAILURES=$script:fails"
exit $script:fails
