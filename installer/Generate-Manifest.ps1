param (
    [Parameter(Mandatory=$true)]
    [string]$PublishDirectory,

    [Parameter(Mandatory=$true)]
    [string]$ManifestPath
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $ManifestPath)) {
    Write-Error "Manifest file '$ManifestPath' does not exist."
    exit 1
}

if (-not (Test-Path $PublishDirectory)) {
    Write-Error "Publish directory '$PublishDirectory' does not exist."
    exit 1
}

$manifestContent = Get-Content $ManifestPath -Raw | ConvertFrom-Json

function Get-FileHashString([string]$filePath) {
    if (Test-Path $filePath) {
        return (Get-FileHash -Path $filePath -Algorithm SHA256).Hash.ToLower()
    }
    return "FILE_NOT_FOUND"
}

# Update component hashes based on expected file paths in the publish directory
if ($manifestContent.components.CastingCore) {
    $corePath = Join-Path $PublishDirectory "CastingCore.exe"
    if (Test-Path $corePath) {
        $manifestContent.components.CastingCore.sha256 = Get-FileHashString $corePath
    }
}

if ($manifestContent.components.AndroidServer) {
    # Assuming android server is pushed to publish/platform-tools/scrcpy-server or similar
    # Using a placeholder path as exact location wasn't specified
    $serverPath = Join-Path $PublishDirectory "scrcpy-server.jar"
    if (Test-Path $serverPath) {
        $manifestContent.components.AndroidServer.sha256 = Get-FileHashString $serverPath
    }
}

if ($manifestContent.components.ADB) {
    $adbPath = Join-Path $PublishDirectory "platform-tools\adb.exe"
    if (Test-Path $adbPath) {
        $manifestContent.components.ADB.sha256 = Get-FileHashString $adbPath
    }
}

$manifestContent | ConvertTo-Json -Depth 5 | Set-Content $ManifestPath

Write-Host "Release manifest updated with SHA256 hashes."
