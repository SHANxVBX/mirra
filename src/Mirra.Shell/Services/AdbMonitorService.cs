using System.Diagnostics;
using System.Reactive.Linq;
using System.Reactive.Subjects;
using Mirra.Shell.Models;
using Microsoft.Extensions.Logging;

namespace Mirra.Shell.Services;

/// <summary>
/// Polls `adb devices -l` every second and emits a stream of connected device lists.
/// Wraps the bundled adb.exe (never touches system PATH).
/// </summary>
public sealed class AdbMonitorService : IDisposable
{
    private readonly ILogger<AdbMonitorService>      _logger;
    private readonly string                           _adbPath;
    private readonly BehaviorSubject<IReadOnlyList<DeviceInfo>> _subject =
        new(Array.Empty<DeviceInfo>());
    private CancellationTokenSource? _cts;
    private Task?                    _pollTask;

    public IObservable<IReadOnlyList<DeviceInfo>> Devices => _subject.AsObservable();
    public IReadOnlyList<DeviceInfo> CurrentDevices       => _subject.Value;

    public AdbMonitorService(ILogger<AdbMonitorService> logger)
    {
        _logger  = logger;
        // Bundled adb.exe lives next to the Shell executable
        _adbPath = Path.Combine(AppContext.BaseDirectory, "platform-tools", "adb.exe");
    }

    public void Start()
    {
        _cts      = new CancellationTokenSource();
        _pollTask = Task.Run(() => PollLoopAsync(_cts.Token));
        _logger.LogInformation("ADB monitor started. Using: {AdbPath}", _adbPath);
    }

    public void Stop()
    {
        _cts?.Cancel();
        _pollTask?.GetAwaiter().GetResult();
    }

    private async Task PollLoopAsync(CancellationToken ct)
    {
        while (!ct.IsCancellationRequested)
        {
            try
            {
                var devices = await RunAdbDevicesAsync(ct);
                _subject.OnNext(devices);
            }
            catch (OperationCanceledException) { break; }
            catch (Exception ex)
            {
                _logger.LogWarning(ex, "ADB poll error");
            }

            await Task.Delay(TimeSpan.FromSeconds(1), ct).ContinueWith(_ => { });
        }
    }

    private async Task<IReadOnlyList<DeviceInfo>> RunAdbDevicesAsync(CancellationToken ct)
    {
        var psi = new ProcessStartInfo(_adbPath, "devices -l")
        {
            RedirectStandardOutput = true,
            RedirectStandardError  = true,
            UseShellExecute        = false,
            CreateNoWindow         = true
        };

        using var proc = Process.Start(psi) ?? throw new InvalidOperationException("adb start failed");
        string output = await proc.StandardOutput.ReadToEndAsync(ct);
        await proc.WaitForExitAsync(ct);

        return ParseDeviceList(output);
    }

    internal static IReadOnlyList<DeviceInfo> ParseDeviceList(string rawOutput)
    {
        var devices = new List<DeviceInfo>();
        bool firstLine = true;

        foreach (var line in rawOutput.Split('\n', StringSplitOptions.RemoveEmptyEntries))
        {
            if (firstLine) { firstLine = false; continue; } // skip header

            var trimmed = line.Trim();
            if (string.IsNullOrWhiteSpace(trimmed)) continue;

            var parts = trimmed.Split('\t');
            if (parts.Length < 2) continue;

            string serial    = parts[0].Trim();
            string stateRaw  = parts[1].Trim().Split(' ')[0];
            string model     = ExtractModel(trimmed);

            DeviceConnectionState state = stateRaw switch
            {
                "device"       => DeviceConnectionState.Ready,
                "unauthorized" => DeviceConnectionState.Unauthorized,
                "offline"      => DeviceConnectionState.Offline,
                _              => DeviceConnectionState.Detected
            };

            devices.Add(new DeviceInfo { Serial = serial, State = state, Model = model });
        }
        return devices;
    }

    private static string ExtractModel(string line)
    {
        const string prefix = "model:";
        int idx = line.IndexOf(prefix, StringComparison.OrdinalIgnoreCase);
        if (idx < 0) return string.Empty;

        string rest = line[(idx + prefix.Length)..];
        return rest.Split(' ')[0].Trim();
    }

    public void Dispose()
    {
        Stop();
        _subject.Dispose();
        _cts?.Dispose();
    }
}
