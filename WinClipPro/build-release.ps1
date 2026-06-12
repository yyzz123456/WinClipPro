# WinClip Pro Release Builder
# Prerequisites: Java backend compiled by IntelliJ, C++ DLL built as x64 Release in VS
param(
    [string]$Version = "1.0.0",
    [string]$OutputDir = "release",
    [switch]$SelfContained
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot

$Label = if ($SelfContained) { "Self-contained" } else { "Framework-dependent" }
Write-Host "=== Building WinClip Pro v$Version ($Label) ===" -ForegroundColor Cyan

# 1. Clean output directory
Write-Host "[1/7] Cleaning output directory..." -ForegroundColor Yellow
if (Test-Path $OutputDir) { Remove-Item -Recurse -Force $OutputDir }

# 2. Publish WPF
if ($SelfContained) {
    Write-Host "[2/7] Publishing WPF app (self-contained)..." -ForegroundColor Yellow
    dotnet publish -c Release -o $OutputDir --self-contained true -r win-x64
} else {
    Write-Host "[2/7] Publishing WPF app (framework-dependent)..." -ForegroundColor Yellow
    dotnet publish -c Release -o $OutputDir
}
if ($LASTEXITCODE -ne 0) { throw "Publish failed" }

# 3. Copy C++ DLL
Write-Host "[3/7] Copying native DLLs..." -ForegroundColor Yellow
$DllSrc = "$Root\x64\Release\WinClipHook.dll"
if (Test-Path $DllSrc) {
    Copy-Item $DllSrc $OutputDir -Force
} else {
    Write-Warning "WinClipHook.dll not found at $DllSrc - build it in VS first (Release x64)"
}

# 4. Copy VC++ runtime DLLs (self-contained only)
if ($SelfContained) {
    Write-Host "[4/7] Copying VC++ runtime DLLs..." -ForegroundColor Yellow
    $VcRedistDlls = @("vcruntime140.dll", "msvcp140.dll")
    $VcSources = @(
        "$env:SystemRoot\System32",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\14.42.34433\x64\Microsoft.VC143.CRT"
    )
    foreach ($dll in $VcRedistDlls) {
        $copied = $false
        foreach ($src in $VcSources) {
            $path = Join-Path $src $dll
            if (Test-Path $path) {
                Copy-Item $path $OutputDir -Force
                Write-Host "  $dll"
                $copied = $true
                break
            }
        }
        if (-not $copied) { Write-Warning "  $dll not found - users may need VC++ Redist" }
    }
} else {
    Write-Host "[4/7] Skipping VC++ runtime (framework-dependent mode)" -ForegroundColor DarkGray
}

# 5. Copy Java backend
Write-Host "[5/7] Copying Java backend..." -ForegroundColor Yellow
$JavaOut = "$OutputDir\JavaBackend"
$JavaSrc = "$Root\JavaBackend"
New-Item -ItemType Directory -Path "$JavaOut\out\production\JavaBackend" -Force | Out-Null
Copy-Item "$JavaSrc\out\production\JavaBackend\*" "$JavaOut\out\production\JavaBackend\" -Recurse -Force
New-Item -ItemType Directory -Path "$JavaOut\lib" -Force | Out-Null
Copy-Item "$JavaSrc\lib\*" "$JavaOut\lib\" -Force

# 6. Bundle minimal JRE via jlink (self-contained only)
if ($SelfContained) {
    Write-Host "[6/7] Creating bundled JRE via jlink..." -ForegroundColor Yellow
    $JreOut = "$JavaOut\jre"
    $jlinkModules = "java.base,java.sql,java.logging,java.naming"
    $jlinkArgs = @(
        "jlink", "--compress=2", "--no-header-files", "--no-man-pages",
        "--add-modules", $jlinkModules,
        "--output", "`"$JreOut`""
    )
    $jlinkCmd = "jlink --compress=2 --no-header-files --no-man-pages --add-modules $jlinkModules --output `"$JreOut`""
    Write-Host "  $jlinkCmd"
    cmd /c $jlinkCmd 2>&1
    if ($LASTEXITCODE -ne 0) { throw "jlink failed - make sure JDK with jlink is installed" }
    Write-Host "  JRE size: $([math]::Round(((Get-ChildItem $JreOut -Recurse | Measure-Object -Property Length -Sum).Sum / 1MB), 1)) MB"
} else {
    Write-Host "[6/7] Skipping JRE bundle (framework-dependent mode)" -ForegroundColor DarkGray
}

# 7. Remove debug symbols
Write-Host "[7/7] Removing PDB files..." -ForegroundColor Yellow
Get-ChildItem -Path $OutputDir -Filter *.pdb -Recurse | Remove-Item -Force -ErrorAction SilentlyContinue

# Create zip
Write-Host "Creating release zip..." -ForegroundColor Yellow
$Suffix = if ($SelfContained) { "-self-contained" } else { "" }
$ZipName = "WinClipPro-v$Version$Suffix.zip"
Compress-Archive -Path $OutputDir\* -DestinationPath $ZipName -Force

Write-Host "=== Done ===" -ForegroundColor Green
Write-Host "Release zip: $((Get-Item $ZipName).FullName) ($([math]::Round((Get-Item $ZipName).Length / 1MB, 1)) MB)"
Write-Host "Folder: $((Get-Item $OutputDir).FullName)"
Write-Host ""
if (-not $SelfContained) {
    Write-Host "For self-contained (no-dependency) build, add -SelfContained:"
    Write-Host "  .\build-release.ps1 -SelfContained -Version $Version"
}
