# proto-smoke: socket-level smoke test without a game client.
# login -> char auth -> char create -> char select -> map enter -> clean quit.
#
# Convention (same as the other tools/* checks): standalone, NOT wired into
# any build on purpose. Run by hand against servers YOU booted yourself
# (this script never starts services). See doc/PROTO-SMOKE.md for
# preparation, execution and cleanup.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools/proto-smoke.ps1 -User <test-user> -Pass <test-pass> -Char <test-char> [-Slot 1]
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
  [int]$ClientVer = 20211103,
  [int]$Slot = 0
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
# Positive evidence for the char list: the 0x6b body must carry at least one
# entry AND contain the expected name as a 24-byte NUL-padded field. A bare
# packet ID proves nothing (an empty list also arrives as 0x6b). Returns
# $false on short/truncated buffers without throwing.
function ParseCharList([byte[]]$body, [string]$name) {
  if ($body -eq $null -or $body.Length -lt 47) { return $false }
  $regionStart = 23
  $regionLen = $body.Length - $regionStart
  if ($regionLen -lt 24) { return $false }
  $pat = New-Object byte[] 24
  $e = [Text.Encoding]::ASCII.GetBytes($name)
  [Array]::Copy($e, 0, $pat, 0, [Math]::Min($e.Length, 24))
  for ($i = 0; $i -le ($regionLen - 24); $i++) {
    $hit = $true
    for ($j = 0; $j -lt 24; $j++) {
      if ($body[$regionStart + $i + $j] -ne $pat[$j]) { $hit = $false; break }
    }
    if ($hit) { return $true }
  }
  return $false
}
function DrainCollect($s, $ms) {
  $end = (Get-Date).AddMilliseconds($ms)
  $all = New-Object System.Collections.Generic.List[byte]
  try { while ((Get-Date) -lt $end) { while ($s.DataAvailable) { $b = New-Object byte[] 8192; $r = $s.Read($b, 0, 8192); for ($k = 0; $k -lt $r; $k++) { $all.Add($b[$k]) } }; Start-Sleep -Milliseconds 100 } } catch { }
  return [byte[]]$all.ToArray()
}
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
"login: AID=$AID char=$chip`:$chport"
# L1/L2 are session tokens: used in auth packets below, never printed.
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
Step "2b-char-list" ($lst -ne $null -and (ParseCharList $lst.data $Char))

# 3. Create char 0xa39 in slot $Slot, then read the server verdict instead of
# blindly draining: 0xb6f (accept) vs 0x6e (refuse + error code). No verdict
# within the window is a FAIL (unknown outcome, never assumed success).
$mk = (U16 0xa39) + (Str $Char 24) + [byte[]]@($Slot) + (U16 1) + (U16 1) + (U32 0) + [byte[]]@(1)
SendBytes $s $mk
$cr = DrainCollect $s 2500
if ($cr.Length -ge 2) {
  $rid = [BitConverter]::ToUInt16($cr, 0)
  if ($rid -eq 0xb6f) { Step "3-create-ack" $true }
  elseif ($rid -eq 0x6e -and $cr.Length -ge 3) { "create-refused error=0x{0:x2}" -f $cr[2]; Step "3-create-ack" $false }
  else { "create-unexpected reply=0x{0:x4}" -f $rid; Step "3-create-ack" $false }
} else {
  Step "3-create-ack" $false
}

# 4. Select slot $Slot, expect 0xac5 map notify. NOTE: 0xac5 has NO length field
# (fixed 156 bytes), so it is read by size, not with ReadPkt.
SendBytes $s ((U16 0x66) + [byte[]]@($Slot))
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
