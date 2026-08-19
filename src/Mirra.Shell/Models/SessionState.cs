namespace Mirra.Shell.Models;

public enum SessionState
{
    Idle,
    AdbSetup,
    ServerInstall,
    Tunneling,
    Streaming,
    Recovering,
    Stopped,
    Error
}

public sealed class SessionStatus
{
    public string    SessionId    { get; init; } = string.Empty;
    public string    DeviceSerial { get; init; } = string.Empty;
    public SessionState State    { get; init; } = SessionState.Idle;
    public string?   ErrorMessage { get; init; }
    public string?   ErrorLayer   { get; init; }
}

public sealed class HealthStatus
{
    public float Fps            { get; init; }
    public int   DecodeMs       { get; init; }
    public int   FrameDropCount { get; init; }
    public int   AudioLatencyMs { get; init; }
    public DateTimeOffset Timestamp { get; init; } = DateTimeOffset.UtcNow;
}
