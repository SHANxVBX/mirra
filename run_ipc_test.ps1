$ErrorActionPreference = "Stop"

Write-Host "Building Casting.Core..."
cd src\Casting.Core
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}
cd build
cmake ..
cmake --build .

Write-Host "`nRunning C# Tests..."
cd ..\..\Mirra.Shell.Tests
dotnet test --filter "IpcIntegrationTests"
