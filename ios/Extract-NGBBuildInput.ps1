param(
    [string]$IsoPath = (Join-Path ([Environment]::GetFolderPath('UserProfile')) 'Downloads\Ninja Gaiden Black (USA) (En,Ja).iso'),
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
# Read only the XDVDFS descriptor, root directory and one executable. No ISO
# changes, installations, administrator rights, network access or background task.
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path ([IO.Path]::GetDirectoryName($IsoPath)) 'NGB-build-input'
}
$destination = Join-Path $OutputDirectory 'default.xbe'
if (Test-Path -LiteralPath $destination) { throw "Output already exists: $destination" }
$imageStream = [IO.File]::Open($IsoPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
$reader = [IO.BinaryReader]::new($imageStream)
try {
    $descriptor = $null
    [long]$partitionBase = -1
    foreach ($candidate in @(0, 0xFD90, 0x30600, 0xFD90000, 0x18300000)) {
        $imageStream.Position = [long]$candidate + 32 * 2048
        $probe = $reader.ReadBytes(2048)
        if ($probe.Length -eq 2048 -and [Text.Encoding]::ASCII.GetString($probe, 0, 20) -eq 'MICROSOFT*XBOX*MEDIA') {
            $descriptor = $probe
            $partitionBase = $candidate
            break
        }
    }
    if ($partitionBase -lt 0) { throw 'No supported Xbox disc partition found.' }
    [long]$rootSector = [BitConverter]::ToUInt32($descriptor, 20)
    [int]$rootSize = [BitConverter]::ToUInt32($descriptor, 24)
    if ($rootSize -le 0 -or $rootSize -gt 16777216) { throw 'Invalid root directory size.' }
    $imageStream.Position = $partitionBase + $rootSector * 2048
    $directory = $reader.ReadBytes($rootSize)
    if ($directory.Length -ne $rootSize) { throw 'Disc directory is truncated.' }
    [long]$fileSector = -1
    [int]$fileSize = 0
    for ($sectorOffset = 0; $sectorOffset -lt $directory.Length; $sectorOffset += 2048) {
        $limit = [Math]::Min($sectorOffset + 2048, $directory.Length)
        $offset = $sectorOffset
        while ($offset + 14 -le $limit) {
            $left = [BitConverter]::ToUInt16($directory, $offset)
            $right = [BitConverter]::ToUInt16($directory, $offset + 2)
            $nameSize = [int]$directory[$offset + 13]
            if (($left -eq 65535 -and $right -eq 65535) -or $nameSize -eq 0) { break }
            if ($offset + 14 + $nameSize -gt $limit) { throw 'Invalid disc directory entry.' }
            $name = [Text.Encoding]::ASCII.GetString($directory, $offset + 14, $nameSize)
            if ([string]::Equals($name, 'default.xbe', [StringComparison]::OrdinalIgnoreCase)) {
                if (($directory[$offset + 12] -band 16) -ne 0) { throw 'default.xbe is a directory.' }
                $fileSector = [BitConverter]::ToUInt32($directory, $offset + 4)
                $fileSize = [BitConverter]::ToUInt32($directory, $offset + 8)
                break
            }
            $offset += (14 + $nameSize + 3) -band -4
        }
        if ($fileSector -ge 0) { break }
    }
    if ($fileSector -lt 0 -or $fileSize -ne 4571136) { throw 'Supported retail USA executable not found.' }
    $imageStream.Position = $partitionBase + $fileSector * 2048
    $executable = $reader.ReadBytes($fileSize)
    if ($executable.Length -ne $fileSize -or [Text.Encoding]::ASCII.GetString($executable, 0, 4) -ne 'XBEH') {
        throw 'Executable is truncated or invalid.'
    }
    [IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
    [IO.File]::WriteAllBytes($destination, $executable)
    Write-Output "Created: $destination"
    Write-Output "Bytes: $($executable.Length)"
    Write-Output 'The original ISO was opened read-only and was not changed.'
    Write-Output 'Upload only default.xbe as the private build input. Keep the complete ISO for game data.'
} finally {
    $reader.Dispose()
}
