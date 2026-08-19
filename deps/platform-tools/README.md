# deps/platform-tools

This directory contains the **bundled Android Platform Tools** (Windows x64) required for Mirra's USB ADB communication.

## Required files
- `adb.exe`
- `AdbWinApi.dll`
- `AdbWinUsbApi.dll`
- `libwinpthread-1.dll`

## How to populate

Download the official Google Platform Tools for Windows:

```
https://dl.google.com/android/repository/platform-tools-latest-windows.zip
```

Verify SHA-256 against `release-manifest.json` before committing.

> **Note:** Only the files listed above are required. Do NOT commit the full
> platform-tools directory (fastboot, etc.) to avoid unnecessary binary bloat.

## Licence

Android Platform Tools are distributed under the Android Software Development Kit License Agreement.
