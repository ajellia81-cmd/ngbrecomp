param(
    [string]$IsoPath = (Join-Path ([Environment]::GetFolderPath('UserProfile')) 'Downloads\Ninja Gaiden Black (USA) (En,Ja).iso'),
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
# No installation, administrator rights, ISO writes or network access.
# Extract only three known font paths, preserving their relative names.
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path ([IO.Path]::GetDirectoryName($IsoPath)) 'NGB-font-input'
}
if (Test-Path -LiteralPath $OutputDirectory) { throw "Output already exists: $OutputDirectory" }
$stream = [IO.File]::Open($IsoPath,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
$reader = [IO.BinaryReader]::new($stream)
$created = $false
try {
    [long]$partition = -1
    $descriptor = $null
    foreach ($candidate in @(0,0xFD90,0x30600,0xFD90000,0x18300000)) {
        if ([long]$candidate + 33*2048 -gt $stream.Length) { continue }
        $stream.Position = [long]$candidate + 32*2048
        $probe = $reader.ReadBytes(2048)
        if ($probe.Length -eq 2048 -and [Text.Encoding]::ASCII.GetString($probe,0,20) -eq 'MICROSOFT*XBOX*MEDIA') {
            $partition = $candidate; $descriptor = $probe; break
        }
    }
    if ($partition -lt 0) { throw 'No supported Xbox disc partition found.' }
    function Read-Directory([long]$Sector,[long]$Size) {
        [long]$position = $partition + $Sector*2048
        if ($Size -lt 14 -or $Size -gt 16777216 -or $position -lt 0 -or $position+$Size -gt $stream.Length) {
            throw 'Invalid Xbox directory extent.'
        }
        $stream.Position = $position
        $bytes = $reader.ReadBytes([int]$Size)
        if ($bytes.Length -ne $Size) { throw 'Truncated Xbox directory.' }
        return ,$bytes
    }
    function Find-Entry([byte[]]$Directory,[string]$Name) {
        $pending = [Collections.Generic.Stack[int]]::new()
        $seen = [Collections.Generic.HashSet[int]]::new()
        $pending.Push(0)
        while ($pending.Count) {
            $offset = $pending.Pop()
            if (-not $seen.Add($offset)) { throw 'Cyclic Xbox directory tree.' }
            if ($offset -lt 0 -or $offset+14 -gt $Directory.Length) { throw 'Invalid Xbox directory node.' }
            $left = [BitConverter]::ToUInt16($Directory,$offset)
            $right = [BitConverter]::ToUInt16($Directory,$offset+2)
            $length = [int]$Directory[$offset+13]
            if ($length -eq 0 -or ($left -eq 65535 -and $right -eq 65535)) { continue }
            if ($offset+14+$length -gt $Directory.Length) { throw 'Invalid Xbox filename.' }
            $entryName = [Text.Encoding]::ASCII.GetString($Directory,$offset+14,$length)
            if ([string]::Equals($entryName,$Name,[StringComparison]::OrdinalIgnoreCase)) {
                return [pscustomobject]@{
                    Sector = [long][BitConverter]::ToUInt32($Directory,$offset+4)
                    Size = [long][BitConverter]::ToUInt32($Directory,$offset+8)
                    Folder = ($Directory[$offset+12] -band 16) -ne 0
                }
            }
            if ($left -and $left -ne 65535) { $pending.Push([int]$left*4) }
            if ($right -and $right -ne 65535) { $pending.Push([int]$right*4) }
        }
        return $null
    }
    $root = Read-Directory ([BitConverter]::ToUInt32($descriptor,20)) ([BitConverter]::ToUInt32($descriptor,24))
    $targets = @('Media/font.ttf','Media/font.xpr','Media/Fonts/kfont.xpr')
    $found = @(); $missing = @(); [long]$total = 0
    foreach ($path in $targets) {
        $parts = $path.Split('/')
        $table = $root; $entry = $null
        for ($i=0;$i -lt $parts.Length;$i++) {
            $entry = Find-Entry $table $parts[$i]
            if (-not $entry) { break }
            if ($i -lt $parts.Length-1) {
                if (-not $entry.Folder) { throw "Expected directory in $path" }
                $table = Read-Directory $entry.Sector $entry.Size
            }
        }
        if (-not $entry) { $missing += $path; continue }
        [long]$position = $partition + $entry.Sector*2048
        if ($entry.Folder -or $entry.Size -le 0 -or $entry.Size -gt 33554432 -or $position+$entry.Size -gt $stream.Length) {
            throw "Invalid or oversized font: $path"
        }
        $total += $entry.Size
        if ($total -gt 100663296) { throw 'Font input exceeds the extraction limit.' }
        $found += [pscustomobject]@{ Path=$path; Position=$position; Size=$entry.Size }
    }
    if (-not $found.Count) { throw 'No expected font files found. Nothing was extracted.' }
    [IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
    $created = $true
    $manifest = @('NGB font-only input; source ISO opened read-only.')
    foreach ($file in $found) {
        $destination = Join-Path $OutputDirectory ($file.Path.Replace('/',[IO.Path]::DirectorySeparatorChar))
        [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
        $stream.Position = $file.Position
        $data = $reader.ReadBytes([int]$file.Size)
        if ($data.Length -ne $file.Size) { throw "Truncated font: $($file.Path)" }
        [IO.File]::WriteAllBytes($destination,$data)
        $hash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash
        $manifest += "$($file.Path) $($file.Size) SHA256=$hash"
    }
    foreach ($path in $missing) { $manifest += "Missing: $path" }
    $manifestPath = Join-Path $OutputDirectory 'font-input-manifest.txt'
    [IO.File]::WriteAllLines($manifestPath,$manifest)
    $zip = Join-Path $OutputDirectory 'NGB-font-input.zip'
    Compress-Archive -LiteralPath @((Join-Path $OutputDirectory 'Media'),$manifestPath) -DestinationPath $zip
    Write-Output "Created: $zip"
    Write-Output "ZIP bytes: $((Get-Item -LiteralPath $zip).Length)"
    Write-Output 'Upload only NGB-font-input.zip privately, never to the public repository.'
    Write-Output 'The original ISO was not changed. No program was installed.'
} catch {
    if ($created) { Remove-Item -LiteralPath $OutputDirectory -Recurse -Force }
    throw
} finally {
    $reader.Dispose()
}
