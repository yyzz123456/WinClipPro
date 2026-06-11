# WinClip Pro Release Builder
# Prerequisites: Java backend compiled by IntelliJ, C++ DLL built as x64 Release in VS
param(
    [string]$Version = "1.0.0",
    [string]$OutputDir = "release"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot

Write-Host "=== Building WinClip Pro v$Version ===" -ForegroundColor Cyan

# 1. Publish WPF as single-file EXE
Write-Host "[1/3] Publishing WPF app..." -ForegroundColor Yellow
dotnet publish -c Release -o $OutputDir
if ($LASTEXITCODE -ne 0) { throw "Publish failed" }

# 2. Copy C++ DLL
Write-Host "[2/3] Copying native DLLs..." -ForegroundColor Yellow
$DllSrc = "$Root\x64\Release\WinClipHook.dll"
if (Test-Path $DllSrc) {
    Copy-Item $DllSrc $OutputDir -Force
} else {
    Write-Warning "WinClipHook.dll not found at $DllSrc - build it in VS first (Release x64)"
}

# 3. Copy Java backend
Write-Host "[3/3] Copying Java backend..." -ForegroundColor Yellow
$JavaOut = "$OutputDir\JavaBackend"
$JavaSrc = "$Root\JavaBackend"
New-Item -ItemType Directory -Path "$JavaOut\out\production\JavaBackend" -Force | Out-Null
Copy-Item "$JavaSrc\out\production\JavaBackend\*" "$JavaOut\out\production\JavaBackend\" -Recurse -Force
New-Item -ItemType Directory -Path "$JavaOut\lib" -Force | Out-Null
Copy-Item "$JavaSrc\lib\*" "$JavaOut\lib\" -Force

# 4. Create zip
Write-Host "Creating release zip..." -ForegroundColor Yellow
$ZipName = "WinClipPro-v$Version.zip"
Compress-Archive -Path $OutputDir\* -DestinationPath $ZipName -Force

Write-Host "=== Done ===" -ForegroundColor Green
Write-Host "Release zip: $((Get-Item $ZipName).FullName)"
Write-Host "Folder: $((Get-Item $OutputDir).FullName)"
Write-Host ""
Write-Host "To create GitHub Release:"
Write-Host "  gh release create v$Version $ZipName --title `"v$Version`" --notes `"First release of WinClip Pro.`""
