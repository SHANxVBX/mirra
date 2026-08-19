using System.Diagnostics;
using Microsoft.Extensions.Logging;

namespace Mirra.Shell.Services;

/// <summary>
/// Spawns and manages the lifecycle of Casting.Core.exe child process.
/// Ensures no orphan processes remain on Shell shutdown.
/// </summary>
public sealed class CoreProcessService : IDisposable
{
    private readonly ILogger<CoreProcessService> _logger;
    private readonly IpcClientService            _ipc;
    private Process?                             _coreProcess;
    private string?                              _currentSessionId;

    public event EventHandler? CoreExited;

    public bool IsRunning => _coreProcess is { HasExited: false };

    public CoreProcessService(ILogger<CoreProcessService> logger, IpcClientService ipc)
    {
        _logger = logger;
        _ipc    = ipc;

        // Ensure cleanup on process exit (even abnormal)
        AppDomain.CurrentDomain.ProcessExit += (_, _) => KillCore();
    }

    public async Task<string> StartAsync(string deviceSerial, CancellationToken ct = default)
    {
        if (IsRunning)
        {
            _logger.LogWarning("CoreProcessService.StartAsync called while Core is already running.");
            return _currentSessionId!;
        }

        _currentSessionId = Guid.NewGuid().ToString("N");
        string pipeName   = $"mirra-core-{_currentSessionId}";
        string corePath   = Path.Combine(AppContext.BaseDirectory, "Casting.Core.exe");

        if (!File.Exists(corePath))
            throw new FileNotFoundException($"Casting.Core.exe not found at {corePath}");

        var psi = new ProcessStartInfo(corePath)
        {
            Arguments      = $"--session {_currentSessionId} --pipe {pipeName} --device {deviceSerial}",
            UseShellExecute = false,
            CreateNoWindow  = true,
            RedirectStandardOutput = false,
            RedirectStandardError  = false
        };

        _coreProcess = new Process { StartInfo = psi, EnableRaisingEvents = true };
        _coreProcess.Exited += OnCoreExited;

        _logger.LogInformation("Starting Casting.Core: session={Session} device={Device}",
            _currentSessionId, deviceSerial);

        _coreProcess.Start();

        // Connect IPC client to the pipe Core creates
        await _ipc.ConnectAsync(pipeName, ct);

        // Send StartSession to transition Core from Idle to AdbSetup
        await _ipc.SendAsync(new
        {
            v       = 1,
            session = _currentSessionId,
            type    = "StartSession",
            ts      = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
            payload = new
            {
                deviceSerial = deviceSerial,
                quality      = "high",
                audioSource  = "output"
            }
        }, ct);

        return _currentSessionId;
    }

    public async Task StopAsync(CancellationToken ct = default)
    {
        if (_coreProcess is null || _coreProcess.HasExited) return;

        _logger.LogInformation("Sending StopSession to Casting.Core.");

        // Send graceful stop command
        await _ipc.SendAsync(new
        {
            v       = 1,
            session = _currentSessionId,
            type    = "StopSession",
            ts      = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
            payload = new { }
        }, ct);

        // Give Core 5 seconds to shut down gracefully
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(5));
        try { await _coreProcess.WaitForExitAsync(timeout.Token); }
        catch (OperationCanceledException)
        {
            _logger.LogWarning("Core did not exit within 5s — killing.");
            KillCore();
        }
    }

    private void KillCore()
    {
        try { _coreProcess?.Kill(entireProcessTree: true); }
        catch (Exception ex) { _logger.LogError(ex, "Failed to kill Casting.Core"); }
    }

    private void OnCoreExited(object? sender, EventArgs e)
    {
        _logger.LogWarning("Casting.Core exited with code {Code}.",
            _coreProcess?.ExitCode);
        CoreExited?.Invoke(this, EventArgs.Empty);
    }

    public void Dispose()
    {
        KillCore();
        _coreProcess?.Dispose();
        _ipc.Dispose();
    }
}
