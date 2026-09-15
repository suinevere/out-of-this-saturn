param([string]$BaseIso,[string]$DataDir,[string]$OutDir,[string]$Name,
      [string]$Xorriso,[string]$Iso2raw)

$IpBinSize = 32768

function ConvertTo-CygPath([string]$p) {
    $full = if ([System.IO.Path]::IsPathRooted($p)) { $p }
            else { [System.IO.Path]::Combine((Get-Location).Path, $p) }
    $full = [System.IO.Path]::GetFullPath($full)
    if ($full -notmatch '^[A-Za-z]:') { return $full.Replace('\', '/') }
    return '/cygdrive/' + $full.Substring(0, 1).ToLower() + $full.Substring(2).Replace('\', '/')
}

function Read-Head([string]$Path) {
    $buf = New-Object byte[] $IpBinSize
    $fs = [System.IO.File]::OpenRead($Path)
    try {
        $read = 0
        while ($read -lt $IpBinSize) {
            $n = $fs.Read($buf, $read, $IpBinSize - $read)
            if ($n -le 0) { break }
            $read += $n
        }
        if ($read -lt $IpBinSize) { Write-Error "$Path is shorter than IP.BIN"; exit 1 }
    } finally { $fs.Dispose() }
    return $buf
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$inj = Join-Path $OutDir "$Name`_injected.iso"

$ip = Read-Head $BaseIso

$files = Get-ChildItem -LiteralPath $DataDir -File |
         Where-Object { $_.Name -match '^(bank[0-9a-f]{2}|memlist\.bin)$' } |
         Sort-Object Name
if ($files.Count -ne 14) {
    Write-Error "expected 14 data files in $DataDir, found $($files.Count) -- run data.bat first"
    exit 1
}
$xargs = @('-indev', (ConvertTo-CygPath $BaseIso), '-outdev', (ConvertTo-CygPath $inj),
           '-rockridge', 'off', '-joliet', 'off')
foreach ($f in $files) { $xargs += @('-map', (ConvertTo-CygPath $f.FullName), "/$($f.Name.ToUpper())") }
$xargs += '-commit'

$xlog = & $Xorriso @xargs 2>&1
if ($LASTEXITCODE -ne 0) {
    $xlog | ForEach-Object { Write-Host $_ }
    Write-Error "xorriso injection failed"; exit 1
}
if (-not (Test-Path $inj) -or (Get-Item $inj).Length -le $IpBinSize) {
    Write-Error "xorriso produced no injected ISO"; exit 1
}

$fs = [System.IO.File]::OpenWrite($inj)
try { $fs.Write($ip, 0, $IpBinSize) } finally { $fs.Dispose() }

if (Compare-Object $ip (Read-Head $inj)) { Write-Error "IP.BIN not preserved"; exit 1 }

& $Iso2raw $inj -o "$OutDir\$Name.bin"
if ($LASTEXITCODE -ne 0) { Write-Error "iso2raw conversion failed"; exit 1 }

"FILE `"$Name.bin`" BINARY","  TRACK 01 MODE1/2352","    INDEX 01 00:00:00" |
    Set-Content "$OutDir\$Name.cue"

Remove-Item -Force -Path $inj
Write-Host "Injected $($files.Count) data files -> $OutDir\$Name.bin"
