param([Parameter(Mandatory=$true)][string]$Staging)
$ErrorActionPreference = 'Stop'

# Keep the per-user installation self-contained. windeployqt supplies a separate
# redistributable installer by default, which would require administrator access.
# https://learn.microsoft.com/en-us/cpp/windows/choosing-a-deployment-method
$runtimeDirectory = Join-Path $env:VCToolsRedistDir 'x64/Microsoft.VC143.CRT'
if (!(Test-Path $runtimeDirectory)) { throw "Missing redistributable CRT: $runtimeDirectory" }
Copy-Item "$runtimeDirectory/*.dll" "$Staging/bin/"
Remove-Item "$Staging/bin/vc_redist.x64.exe" -ErrorAction SilentlyContinue

# Windows uses the native WebView2 backend, not the optional Chromium backend.
Get-ChildItem $Staging -Recurse -Filter qtwebview_webengine.dll | Remove-Item
$webviewMetadata = Get-ChildItem $Staging -Recurse -Filter qmldir | Where-Object { $_.Directory.Name -eq 'QtWebView' }
foreach ($metadata in $webviewMetadata) {
    $content = Get-Content $metadata.FullName | Where-Object { $_ -notmatch '^depends QtWebEngine ' }
    Set-Content $metadata.FullName $content -Encoding utf8
}
if (!(Get-ChildItem $Staging -Recurse -Filter qtwebview_webview2.dll)) {
    throw 'Native WebView2 plugin is missing'
}

$noticeDirectory = "$Staging/share/licenses/neden"
@'
Microsoft Visual C++ Runtime
Copyright Microsoft Corporation. All rights reserved.
The release CRT DLLs are redistributed from Visual Studio's redistributable directory.
They retain Microsoft's license and are not covered by Neden's MIT license.
Redistribution list: https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution
App-local runtime updates are delivered with Neden releases.
'@ | Set-Content "$noticeDirectory/Microsoft-CRT.txt" -Encoding utf8

# Reject missing DLLs before packaging, including optional Qt plugins. Windows
# system/API-set libraries are supplied by the supported Windows installation.
$bundledLibraries = @{}
Get-ChildItem $Staging -Recurse -Filter *.dll | ForEach-Object { $bundledLibraries[$_.Name.ToLowerInvariant()] = $true }
$binaries = Get-ChildItem $Staging -Recurse -File | Where-Object { $_.Extension -in '.exe', '.dll' }
foreach ($binary in $binaries) {
    $dependencies = & dumpbin /nologo /dependents $binary.FullName
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect $($binary.FullName)" }
    foreach ($line in $dependencies) {
        if ($line -notmatch '^\s+([a-zA-Z0-9_.+-]+\.dll)\s*$') { continue }
        $name = $Matches[1].ToLowerInvariant()
        if ($bundledLibraries.ContainsKey($name) -or $name -match '^(api|ext)-ms-') { continue }
        if ($name -match '^(msvcp|vcruntime|concrt)') { throw "Unbundled compiler runtime: $name" }
        if (!(Test-Path "$env:WINDIR/System32/$name")) { throw "Missing $name required by $($binary.Name)" }
    }
}
Get-ChildItem $Staging -Recurse -File | ForEach-Object {
    $relative = [IO.Path]::GetRelativePath($Staging, $_.FullName)
    "$((Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant())  $relative"
} | Set-Content "$Staging/package-files.sha256" -Encoding utf8
