param([Parameter(Mandatory=$true)][string]$Runtime)
$ErrorActionPreference = 'Stop'
$binaryDirectory = Join-Path $Runtime 'bin'
$library = @(Get-ChildItem "$binaryDirectory/*mpv*.dll")
if ($library.Count -ne 1) { throw 'Expected exactly one source-built mpv DLL' }
$exports = & dumpbin /exports $library[0].FullName
if ($LASTEXITCODE) { throw 'Could not read libmpv exports' }
$names = $exports | Where-Object { $_ -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(mpv_\w+)' } | ForEach-Object { $Matches[1] }
if (!$names) { throw 'No libmpv C API exports found' }
@("LIBRARY $($library[0].Name)", 'EXPORTS') + $names | Set-Content "$Runtime/mpv.def"
lib /def:"$Runtime/mpv.def" /out:"$Runtime/lib/mpv.lib" /machine:x64
if ($LASTEXITCODE) { throw 'Could not generate MSVC import library' }

# Resolve only the actual dependency closure, preserving our source-built codecs.
$pending = [System.Collections.Generic.Queue[string]]::new()
Get-ChildItem "$binaryDirectory/*.dll" | ForEach-Object { $pending.Enqueue($_.FullName) }
$visited = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$copied = @()
while ($pending.Count -gt 0) {
    $binary = $pending.Dequeue()
    if (!$visited.Add($binary)) { continue }
    $dependencies = & dumpbin /dependents $binary
    if ($LASTEXITCODE) { throw "Could not inspect $binary" }
    foreach ($line in $dependencies) {
        if ($line -notmatch '^\s+([\w.+-]+\.dll)\s*$') { continue }
        $name = $Matches[1]
        $destination = Join-Path $binaryDirectory $name
        if (Test-Path $destination) { $pending.Enqueue($destination); continue }
        if ($name -match '^(api-ms-|ext-ms-)' -or (Test-Path "$env:WINDIR/System32/$name")) { continue }
        $source = "C:/msys64/ucrt64/bin/$name"
        if (!(Test-Path $source)) { throw "Unresolved runtime dependency: $name" }
        Copy-Item $source $destination
        $copied += $name
        $pending.Enqueue($destination)
    }
}
$copied | Sort-Object -Unique | Set-Content "$Runtime/dependency-dlls.txt"
