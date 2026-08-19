param (
    [Parameter(Mandatory=$true)]
    [string]$TargetDirectory,

    [Parameter(Mandatory=$false)]
    [string]$CertificateThumbprint,

    [Parameter(Mandatory=$false)]
    [string]$CertificatePath,

    [Parameter(Mandatory=$false)]
    [string]$CertificatePassword,
    
    [string]$TimestampServer = "http://timestamp.digicert.com"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $TargetDirectory)) {
    Write-Error "Target directory '$TargetDirectory' does not exist."
    exit 1
}

$signtoolPath = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe" # Fallback if not in PATH
if (Get-Command "signtool.exe" -ErrorAction SilentlyContinue) {
    $signtoolPath = "signtool.exe"
} elseif (-not (Test-Path $signtoolPath)) {
    # Try to find signtool
    $signtoolPath = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin" -Filter "signtool.exe" -Recurse | Select-Object -First 1).FullName
}

if (-not $signtoolPath -or -not (Test-Path $signtoolPath -ErrorAction SilentlyContinue)) {
    if (Get-Command "signtool.exe" -ErrorAction SilentlyContinue) {
        $signtoolPath = "signtool.exe"
    } else {
        Write-Warning "signtool.exe not found. Skipping signing."
        exit 0
    }
}

$filesToSign = Get-ChildItem -Path $TargetDirectory -Include *.exe, *.dll -Recurse | Where-Object { $_.Length -gt 0 }

if ($filesToSign.Count -eq 0) {
    Write-Host "No executable files found to sign in $TargetDirectory."
    exit 0
}

Write-Host "Found $($filesToSign.Count) files to sign."

$signArgs = @("sign", "/fd", "SHA256", "/tr", $TimestampServer, "/td", "SHA256")

if ($CertificateThumbprint) {
    $signArgs += "/sha1", $CertificateThumbprint
} elseif ($CertificatePath) {
    $signArgs += "/f", $CertificatePath
    if ($CertificatePassword) {
        $signArgs += "/p", $CertificatePassword
    }
} else {
    Write-Warning "No certificate information provided. Skipping signing."
    exit 0
}

foreach ($file in $filesToSign) {
    Write-Host "Signing $($file.FullName)..."
    $fileArgs = $signArgs + $file.FullName
    
    if ($signtoolPath -eq "signtool.exe") {
        & signtool.exe @fileArgs
    } else {
        & $signtoolPath @fileArgs
    }
    
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to sign $($file.FullName)"
        exit $LASTEXITCODE
    }
}

Write-Host "Binary signing completed successfully."
