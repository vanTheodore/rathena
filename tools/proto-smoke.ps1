# proto-smoke: socket-level smoke test without a game client.
# login -> char auth -> char create -> char select -> map enter -> clean quit.
#
# Convention (same as the other tools/* checks): standalone, NOT wired into
# any build on purpose. Run by hand against servers YOU booted yourself
# (this script never starts services). See doc/PROTO-SMOKE.md for
# preparation, execution and cleanup.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools/proto-smoke.ps1 -User <test-user> -Pass <test-pass> -Char <test-char>
# Exit 0 + "FAILURES=0" on success; nonzero exit + FAIL lines on failure.
#
# Security: this tool performs NO destructive operation. It opens sockets,
# creates at most one test character and quits (quit triggers the normal
# server-side save). It never touches a database directly, embeds no
# password and reads no credential file: -User/-Pass/-Char are mandatory
# precisely so nothing personal or secret lives in this file. Point the
# servers at a disposable database; never at production data. The 0x1fb
# char-delete flow is deliberately NOT included (pending, see doc).
param(
  [Parameter(Mandatory=$true)][string]$User,
  [Parameter(Mandatory=$true)][string]$Pass,
  [Parameter(Mandatory=$true)][string]$Char,
  [string]$LoginHost = "127.0.0.1",
  [int]$LoginPort = 6900,
  [int]$ClientVer = 20211103
)
$script:fails = 0
function Step($name, $cond) { if ($cond) { "PASS $name" } else { "FAIL $name"; $script:fails++ } }
function NewConn($port) {
  $c = New-Object System.Net.Sockets.TcpClient
  $c.ReceiveTimeout = 10000; $c.SendTimeout = 5000
  $c.Connect($LoginHost, $port)
  return $c
}
function ReadPkt($s) {
  $h = New-Object byte[] 4
  $r = 0
  try {
    while ($r -lt 2) { $n = $s.Read($h, $r, 2 - $r); if ($n -le 0) { return $null }; $r += $n }
    $id = [BitConverter]::ToUInt16($h, 0)
    while ($r -lt 4) { $n = $s.Read($h, $r, 4 - $r); if ($n -le 0) { return $null }; $r += $n }
    $len = [BitConverter]::ToUInt16($h, 2)
    if ($len -lt 4 -or $len -gt 32768) { return @{ id = $id; len = -1; data = $h } }
    $body = New-Object byte[] ($len - 4)
    $r = 0
    while ($r -lt ($len - 4)) { $n = $s.Read($body, $r, ($len - 4) - $r); if ($n -le 0) { break }; $r += $n }
    return @{ id = $id; len = $len; data = $body }
  } catch { return $null }
}
function Drain($s, $ms) {
  $end = (Get-Date).AddMilliseconds($ms)
  $n = 0
  try { while ((Get-Date) -lt $end) { while ($s.DataAvailable) { $b = New-Object byte[] 8192; $r = $s.Read($b, 0, 8192); $n += $r }; Start-Sleep -Milliseconds 100 } } catch { }
  return $n
}
function SendBytes($s, [byte[]]$p) { $s.Write($p, 0, $p.Length) }
function U16($v) { return [BitConverter]::GetBytes([uint16]$v) }
function U32($v) { return [BitConverter]::GetBytes([uint32]$v) }
function Str($s, $n) { $b = New-Object byte[] $n; $e = [Text.Encoding]::ASCII.GetBytes($s); [Array]::Copy($e, 0, $b, 0, [Math]::Min($e.Length, $n)); return $b }

# 1. Login 0x64
$c = NewConn $LoginPort; $s = $c.GetStream()
$pkt = (U16 0x64) + (U32 $ClientVer) + (Str $User 24) + (Str $Pass 24) + [byte[]]@(0)
SendBytes $s $pkt
$ac = $null; $end = (Get-Date).AddSeconds(10)
while ((Get-Date) -lt $end -and ($ac -eq $null -or ($ac.id -ne 0xac4 -and $ac.id -ne 0x83e))) { $ac = ReadPkt $s }
Step "1-login-accept" ($ac -ne $null -and $ac.id -eq 0xac4)
if ($ac -eq $null -or $ac.id -ne 0xac4 -or $ac.len -lt 224) { "ABORT at login stage"; exit 1 }
$AID = [BitConverter]::ToUInt32($ac.data, 4); $L1 = [BitConverter]::ToUInt32($ac.data, 0); $L2 = [BitConverter]::ToUInt32($ac.data, 8)
$chip = "{0}.{1}.{2}.{3}" -f $ac.data[60], $ac.data[61], $ac.data[62], $ac.data[63]
$chport = [BitConverter]::ToUInt16($ac.data, 64)
"login: AID=$AID L1=$L1 L2=$L2 char=$chip`:$chport"
# NB: keep the login connection OPEN (like a real client) until char auth
# completes: closing it early can disturb the in-memory auth/online state.
$loginConn = $c

# 2. Char auth 0x65. The server first echoes 4 raw bytes (account_id, no
# packet id), then sends the 0x6b char list after login-server confirms auth.
$c = NewConn $chport; $s = $c.GetStream()
# 0x65 is 17 bytes: the 2 bytes at offset 14 are client-reserved (sex stays at 16).
SendBytes $s ((U16 0x65) + (U32 $AID) + (U32 $L1) + (U32 $L2) + [byte[]]@(0, 0) + [byte[]]@(1))
$echo = New-Object byte[] 4; $r = 0
try { while ($r -lt 4) { $n = $s.Read($echo, $r, 4 - $r); if ($n -le 0) { break }; $r += $n } } catch { }
Step "2a-auth-echo" ($r -eq 4 -and [BitConverter]::ToUInt32($echo, 0) -eq $AID)
$lst = $null; $end = (Get-Date).AddSeconds(15)
while ((Get-Date) -lt $end) { $p = ReadPkt $s; if ($p -eq $null) { break }; if ($p.id -eq 0x6b) { $lst = $p; break } }
Step "2b-char-list" ($lst -ne $null)

# 3. Create char 0xa39. Re-runnable: if the name already exists from a
# previous run the server refuses and select below proceeds anyway.
$mk = (U16 0xa39) + (Str $Char 24) + [byte[]]@(0) + (U16 1) + (U16 1) + (U32 0) + [byte[]]@(1)
SendBytes $s $mk
Drain $s 2500 | Out-Null

# 4. Select slot 0, expect 0xac5 map notify. NOTE: 0xac5 has NO length field
# (fixed 156 bytes), so it is read by size, not with ReadPkt.
SendBytes $s ((U16 0x66) + [byte[]]@(0))
$zn = $null; $end = (Get-Date).AddSeconds(15)
$acc = New-Object byte[] 156; $r = 0
try { while ($r -lt 156 -and (Get-Date) -lt $end) { if ($s.DataAvailable) { $n = $s.Read($acc, $r, 156 - $r); if ($n -le 0) { break }; $r += $n } else { Start-Sleep -Milliseconds 100 } } } catch { }
if ($r -eq 156 -and [BitConverter]::ToUInt16($acc, 0) -eq 0xac5) { $zn = @{ id = 0xac5; len = 156; data = $acc[2..155] } }
Step "4-map-notify" ($zn -ne $null)
if ($zn -eq $null -or $zn.len -lt 156) { "ABORT at select stage"; exit 1 }
$CID = [BitConverter]::ToUInt32($zn.data, 0)
$mmap = ([Text.Encoding]::ASCII.GetString($zn.data[4..19])).Trim([char]0)
$mip = "{0}.{1}.{2}.{3}" -f $zn.data[20], $zn.data[21], $zn.data[22], $zn.data[23]
$mport = [BitConverter]::ToUInt16($zn.data, 24)
"map: CID=$CID map=$mmap srv=$mip`:$mport"
$c.Close()

# 5. Map enter. At our PACKETVER the active WantToConnection row is 0x0888
# (classic 19-byte layout, positions 2/6/10/14/18); 0x72 was recycled for
# UseSkillToId long ago. Drain world data, then clean close (triggers
# server-side save path).
$c = NewConn $mport; $s = $c.GetStream()
SendBytes $s ((U16 0x0888) + (U32 $AID) + (U32 $CID) + (U32 $L1) + (U32 0) + [byte[]]@(1))
$n = Drain $s 8000
Step "5-map-data" ($n -gt 100)
"c bytes from map: $n"
$c.Close()
$loginConn.Close()
"FAILURES=$script:fails"
exit $script:fails
