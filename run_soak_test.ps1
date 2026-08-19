$ErrorActionPreference = "Stop"

$iterations = 100
if ($args.Count -gt 0) {
    $iterations = [int]$args[0]
}

Write-Host "Starting Soak Test for $iterations iterations..."

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

for ($i = 1; $i -le $iterations; $i++) {
    Write-Host "--- Soak Test Iteration $i of $iterations ---"
    
    # Run the IPC integration tests to simulate load
    cd src\Mirra.Shell.Tests
    dotnet test --filter "IpcIntegrationTests" --logger:"console;verbosity=quiet"
    cd ..\..
    
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Soak test failed on iteration $i"
        exit $LASTEXITCODE
    }
}

$stopwatch.Stop()
Write-Host "Soak Test completed successfully in $($stopwatch.Elapsed.TotalSeconds) seconds."
