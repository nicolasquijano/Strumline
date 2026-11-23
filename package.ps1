# Packages a Strumline release into dist\: the zip (VST3 bundle + standalone + licence), the installer (when Inno Setup 6 or 7 is installed) and SHA256SUMS.txt.
# The version is read from the top of CMakeLists.txt. See docs\RELEASING.md.
param([switch]$SkipTests)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

$cmake = Get-Content CMakeLists.txt -Raw
$version = [regex]::Match($cmake, 'set\(STRUMLINE_VERSION\s+([0-9.]+)\)').Groups[1].Value
$suffix = [regex]::Match($cmake, 'set\(STRUMLINE_VERSION_SUFFIX\s+"([^"]*)"\)').Groups[1].Value
if (-not $version) { throw 'could not read STRUMLINE_VERSION from CMakeLists.txt' }
$full = "$version$suffix"
Write-Host "Strumline $full"

Write-Host '== build'
& cmd /c "`"$PSScriptRoot\build.cmd`""
if ($LASTEXITCODE -ne 0) { throw 'the build failed' }

if (-not $SkipTests) {
    Write-Host '== tests (strumline_check)'
    & .\build\plugin\strumline_check.exe
    if ($LASTEXITCODE -ne 0) { throw 'strumline_check failed' }
}

$art = 'build\plugin\Strumline_artefacts\Release'
foreach ($p in "$art\VST3\Strumline.vst3\Contents\x86_64-win\Strumline.vst3", "$art\VST3\Strumline.vst3\Contents\x86_64-win\onnxruntime.dll", "$art\Standalone\Strumline.exe", "$art\Standalone\onnxruntime.dll") {
    if (-not (Test-Path $p)) { throw "missing: $p (is ONNX Runtime in place? see README)" }
}

if (Test-Path dist) { Remove-Item dist -Recurse -Force }
New-Item -ItemType Directory dist | Out-Null

Write-Host '== zip'
$stage = "dist\_stage\Strumline-$full"
New-Item -ItemType Directory "$stage\VST3", "$stage\Standalone" | Out-Null
Copy-Item "$art\VST3\Strumline.vst3" "$stage\VST3\Strumline.vst3" -Recurse
Copy-Item "$art\Standalone\*" "$stage\Standalone" -Recurse
Copy-Item LICENSE, THIRD_PARTY_NOTICES.md, README.md, CHANGELOG.md $stage
Set-Content "$stage\INSTALL.txt" @"
Strumline $full (Windows, 64 bit)

VST3 plug-in: copy the folder VST3\Strumline.vst3 to
  C:\Program Files\Common Files\VST3     (all users, needs an administrator)   or
  %LOCALAPPDATA%\Programs\Common\VST3    (only you)
and rescan the plugins in your DAW. onnxruntime.dll has to stay inside the bundle, next to Strumline.vst3 (the chord converter loads it from there).

Standalone application: run Standalone\Strumline.exe (keep the two DLLs next to it). Its settings window has a MIDI output selector.

Strumline is free software under the AGPLv3 (LICENSE); third-party notices are in THIRD_PARTY_NOTICES.md.
"@
Compress-Archive -Path "$stage\*" -DestinationPath "dist\Strumline-$full-win64.zip"
Remove-Item dist\_stage -Recurse -Force

Write-Host '== installer'
$iscc = @("$env:ProgramFiles\Inno Setup 6\ISCC.exe", "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
          "$env:ProgramFiles\Inno Setup 7\ISCC.exe", "${env:ProgramFiles(x86)}\Inno Setup 7\ISCC.exe", "$env:LOCALAPPDATA\Programs\Inno Setup 7\ISCC.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($iscc) {
    & $iscc "/DAppVersion=$version" "/DAppVersionFull=$full" installer\strumline.iss
    if ($LASTEXITCODE -ne 0) { throw 'Inno Setup failed' }
} else {
    Write-Warning 'Inno Setup 6 or 7 is not installed: no installer (the zip is made).'
}

Write-Host '== checksums'
Get-ChildItem dist -File | Get-FileHash -Algorithm SHA256 | ForEach-Object { "$($_.Hash.ToLower())  $(Split-Path $_.Path -Leaf)" } | Set-Content dist\SHA256SUMS.txt
Get-ChildItem dist | Select-Object Name, @{n='MB'; e={[math]::Round($_.Length / 1MB, 2)}} | Format-Table -AutoSize
Write-Host "Done. dist\ has the release $full."
