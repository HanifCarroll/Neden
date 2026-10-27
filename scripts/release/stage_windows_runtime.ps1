param(
    [Parameter(Mandatory=$true)][string]$Runtime,
    [Parameter(Mandatory=$true)][string]$MsysDirectory
)
$ErrorActionPreference = 'Stop'
$binaryDirectory = Join-Path $Runtime 'bin'
# MSVC needs import libraries for every C API the application links directly.
foreach ($component in @('mpv', 'avformat', 'avcodec', 'avutil')) {
    $pattern = if ($component -eq 'mpv') { '*mpv*.dll' } else { "$component-*.dll" }
    $library = @(Get-ChildItem (Join-Path $binaryDirectory $pattern))
    if ($library.Count -ne 1) { throw "Expected exactly one source-built $component DLL" }
    $exports = & dumpbin /exports $library[0].FullName
    if ($LASTEXITCODE) { throw "Could not read $component exports" }
    $prefix = if ($component -eq 'mpv') { 'mpv_' } else { 'av' }
    $names = $exports | Where-Object {
        $_ -match "^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+($prefix\w+)"
    } | ForEach-Object { $Matches[1] }
    if (!$names) { throw "No $component C API exports found" }
    $definition = Join-Path $Runtime "$component.def"
    $importLibrary = Join-Path $Runtime "lib/$component.lib"
    @("LIBRARY $($library[0].Name)", 'EXPORTS') + $names | Set-Content $definition
    lib /def:$definition /out:$importLibrary /machine:x64
    if ($LASTEXITCODE) { throw "Could not generate $component MSVC import library" }
}

# Resolve only the actual dependency closure, preserving our source-built codecs.
$pending = [System.Collections.Generic.Queue[string]]::new()
Get-ChildItem "$binaryDirectory/*.dll" | ForEach-Object { $pending.Enqueue($_.FullName) }
$visited = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$copied = @()
if (Test-Path "$Runtime/dependency-dlls.txt") {
    $copied = @(Get-Content "$Runtime/dependency-dlls.txt" | Where-Object { $_.Trim() })
}
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
        $source = Join-Path $MsysDirectory "ucrt64/bin/$name"
        if (!(Test-Path $source)) { throw "Unresolved runtime dependency: $name" }
        Copy-Item $source $destination
        $copied += $name
        $pending.Enqueue($destination)
    }
}
$copied | Sort-Object -Unique | Set-Content "$Runtime/dependency-dlls.txt"
