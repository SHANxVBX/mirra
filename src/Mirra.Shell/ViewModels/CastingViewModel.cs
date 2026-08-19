using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Mirra.Shell.Models;
using Mirra.Shell.Services;
using Microsoft.Extensions.Logging;
using System.Text.Json.Nodes;

namespace Mirra.Shell.ViewModels;

public sealed partial class CastingViewModel : ObservableObject, IDisposable
{
    private readonly CoreProcessService  _coreService;
    private readonly IpcClientService    _ipc;
    private readonly INavigationService  _navigation;
    private readonly ILogger<CastingViewModel> _logger;
    private CancellationTokenSource?     _cts;
    private Task?                        _messageTask;

    // ── Observables ───────────────────────────────────────────────────────────

    [ObservableProperty] private SessionState _sessionState = SessionState.Idle;
    [ObservableProperty] private string       _sessionStateLabel = "Connecting...";
    [ObservableProperty] private float        _fps = 0f;
    [ObservableProperty] private int          _decodeMs = 0;
    [ObservableProperty] private int          _frameDropCount = 0;
    [ObservableProperty] private bool         _isRecording = false;
    [ObservableProperty] private bool         _isFullscreen = false;
    [ObservableProperty] private string?      _errorMessage = null;

    // HWND reported by Core for HwndHost embedding
    public long CoreHwnd { get; private set; }
    public event EventHandler<long>? CoreHwndReady;

    public CastingViewModel(
        CoreProcessService coreService,
        IpcClientService ipc,
        INavigationService navigation,
        ILogger<CastingViewModel> logger)
    {
        _coreService = coreService;
        _ipc         = ipc;
        _navigation  = navigation;
        _logger      = logger;

        coreService.CoreExited += OnCoreExited;
        StartListening();
    }

    private void StartListening()
    {
        _cts         = new CancellationTokenSource();
        _messageTask = Task.Run(() => ListenAsync(_cts.Token));
    }

    private async Task ListenAsync(CancellationToken ct)
    {
        await foreach (var msg in _ipc.Messages.ReadAllAsync(ct))
        {
            string type = msg["type"]?.GetValue<string>() ?? string.Empty;
            switch (type)
            {
                case "StateChanged":   HandleStateChanged(msg);   break;
                case "HealthTick":     HandleHealthTick(msg);     break;
                case "FirstFrame":     HandleFirstFrame(msg);     break;
                case "Error":          HandleError(msg);          break;
                case "CoreHwnd":       HandleCoreHwnd(msg);      break;
                case "RecordingStarted": IsRecording = true;      break;
                case "RecordingStopped": IsRecording = false;     break;
            }
        }
    }

    private void HandleStateChanged(JsonObject msg)
    {
        string state = msg["payload"]?["state"]?.GetValue<string>() ?? string.Empty;
        SessionState = Enum.TryParse<SessionState>(state, out var s) ? s : SessionState.Idle;
        SessionStateLabel = SessionState switch
        {
            SessionState.AdbSetup      => "Setting up ADB tunnel...",
            SessionState.ServerInstall => "Installing casting server...",
            SessionState.Tunneling     => "Opening video tunnel...",
            SessionState.Streaming     => "Casting",
            SessionState.Recovering    => "Reconnecting...",
            SessionState.Stopped       => "Stopped",
            _                          => "Connecting..."
        };
    }

    private void HandleHealthTick(JsonObject msg)
    {
        var p = msg["payload"];
        if (p is null) return;
        Fps            = p["fps"]?.GetValue<float>()  ?? 0;
        DecodeMs       = p["decodeMs"]?.GetValue<int>() ?? 0;
        FrameDropCount = p["frameDropCount"]?.GetValue<int>() ?? 0;
    }

    private void HandleFirstFrame(JsonObject msg)
    {
        SessionStateLabel = "Casting";
        _logger.LogInformation("First frame received. latencyMs={Lat}",
            msg["payload"]?["latencyMs"]?.GetValue<int>());
    }

    private void HandleError(JsonObject msg)
    {
        ErrorMessage = msg["payload"]?["message"]?.GetValue<string>() ?? "Unknown error";
        SessionState = SessionState.Error;
        _logger.LogError("Core error: {Layer} - {Msg}",
            msg["payload"]?["layer"]?.GetValue<string>(), ErrorMessage);
    }

    private void HandleCoreHwnd(JsonObject msg)
    {
        CoreHwnd = msg["payload"]?["hwnd"]?.GetValue<long>() ?? 0;
        if (CoreHwnd != 0)
        {
            CoreHwndReady?.Invoke(this, CoreHwnd);
        }
    }

    [RelayCommand]
    private async Task StopAsync()
    {
        await _coreService.StopAsync();
        _navigation.NavigateTo<DeviceListViewModel>();
    }

    [RelayCommand]
    private async Task TakeScreenshotAsync()
    {
        var path = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.MyPictures),
            "Mirra", $"screenshot_{DateTime.Now:yyyyMMdd_HHmmss}.png");
        Directory.CreateDirectory(Path.GetDirectoryName(path)!);
        await _ipc.SendAsync(new { v = 1, type = "TakeScreenshot", payload = new { outputPath = path } });
    }

    [RelayCommand]
    private async Task ToggleRecordingAsync()
    {
        if (!IsRecording)
        {
            var path = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.MyVideos),
                "Mirra", $"recording_{DateTime.Now:yyyyMMdd_HHmmss}.mp4");
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            await _ipc.SendAsync(new { v = 1, type = "StartRecording", payload = new { outputPath = path } });
        }
        else
        {
            await _ipc.SendAsync(new { v = 1, type = "StopRecording", payload = new { } });
        }
    }

    private void OnCoreExited(object? sender, EventArgs e)
    {
        if (SessionState != SessionState.Stopped)
        {
            SessionState      = SessionState.Error;
            SessionStateLabel = "Casting.Core exited unexpectedly.";
            ErrorMessage      = "The media engine exited. Check diagnostics for details.";
        }
    }

    public async Task SendTouchAsync(string eventType, float x, float y)
    {
        await _ipc.SendAsync(new { v = 1, type = "SendInput", payload = new { eventType, x, y } });
    }

    public async Task SendKeyAsync(int action, int keyCode, int metaState)
    {
        await _ipc.SendAsync(new { v = 1, type = "SendInput", payload = new { eventType = "key", action, keyCode, metaState } });
    }

    public async Task SendScrollAsync(float x, float y, float scrollX, float scrollY)
    {
        await _ipc.SendAsync(new { v = 1, type = "SendInput", payload = new { eventType = "scroll", x, y, scrollX, scrollY } });
    }

    public void Dispose()
    {
        _cts?.Cancel();
        _messageTask?.GetAwaiter().GetResult();
        _cts?.Dispose();
        _coreService.CoreExited -= OnCoreExited;
    }
}
