# Use a Visual Studio Developer PowerShell/Command Prompt, with CMake and C++ build tools.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
cmake -S $root -B "$root\build" -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
cmake --build "$root\build" --config Release
if ($LASTEXITCODE -ne 0) { throw 'Windows build failed' }
Write-Host "Built $root\build\Release\TokenStatus.dll"
