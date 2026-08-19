# Mirra

**Android-to-Windows ultra-low-latency USB phone casting**

Mirror your Android phone screen to Windows over USB with near-zero latency — no Wi-Fi, no cloud, no account required.

---

## Architecture

```
┌─────────────────────────┐     Named Pipe (JSON)    ┌────────────────────────┐
│    Mirra.Shell (WPF)    │◄────────────────────────►│  Casting.Core (C++)    │
│    .NET 10 / MVVM       │                           │  FFmpeg + SDL3         │
│    Device list, controls│                           │  ADB tunnel mgmt       │
└─────────────────────────┘                           └───────────┬────────────┘
                                                                  │ ADB (USB)
                                                                  ▼
                                                       ┌─────────────────────┐
                                                       │  Android Server JAR  │
                                                       │  (scrcpy fork)       │
                                                       │  H.264 MediaCodec    │
                                                       └─────────────────────┘
```

## Requirements

**Development**
- Windows 10 22H2+ or Windows 11
- Visual Studio 2022 17.8+ (MSVC v143, C++ workload)
- .NET 10 SDK
- vcpkg (bootstrapped by CMake)
- Android SDK / Android Studio (for AndroidServer)

**Runtime**
- Windows 10 22H2+ (build 19041+) x64
- Android device with Developer Options + USB Debugging enabled
- USB cable supporting data transfer

## Building

### Casting.Core (C++)
```powershell
cd src/Casting.Core
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

### Mirra.Shell (.NET)
```powershell
dotnet build src/Mirra.Shell/Mirra.Shell.csproj --configuration Release
```

### Tests
```powershell
# C++ tests
cd src/Casting.Core/build && ctest -C Release --output-on-failure

# .NET tests
dotnet test src/Mirra.Shell.Tests/
```

## Key Design Decisions

| Decision | Choice |
|---|---|
| Native core | C++20, CMake, vcpkg |
| UI shell | C# WPF .NET 10, CommunityToolkit.Mvvm |
| Android server | Fork of scrcpy (Apache 2.0) |
| Video codec | H.264 hardware encoder (MediaCodec) |
| IPC | Named pipe + length-prefixed JSON |
| Installer | Inno Setup (per-user, no elevation) |

## Delivery Phases

- **Phase 0** — Repository scaffold, CMake/vcpkg, DI bootstrap, IPC schema ← _current_
- **Phase 1** — Technical spike: headless ADB+H.264+SDL3 latency proof; WPF device list; Android server fork
- **Phase 2** — Integration alpha: HwndHost embedding, end-to-end IPC, input injection, session state machine
- **Phase 3** — Feature-complete beta: audio, recording, clipboard, diagnostics, accessibility
- **Phase 4** — Release hardening: installer, soak tests, security, compatibility matrix

## Performance Targets

| Metric | Target |
|---|---|
| Glass-to-glass latency (720p60) | p95 ≤ 80 ms |
| First frame (ready device) | p95 ≤ 3 seconds |
| Memory trend over 8 hours | Flat (no growth) |

## Licence

Apache License 2.0 — see [LICENSE](LICENSE).  
Portions derived from [scrcpy](https://github.com/Genymobile/scrcpy) © Genymobile (Apache 2.0).
See [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt) for all redistributed components.
