namespace Mirra.Shell.Models;

public enum DeviceConnectionState
{
    NoDevice,
    Detected,
    Unauthorized,
    Offline,
    Ready
}

public sealed class DeviceInfo
{
    public string Serial { get; init; } = string.Empty;
    public string Model  { get; init; } = string.Empty;
    public DeviceConnectionState State { get; init; } = DeviceConnectionState.NoDevice;

    public string DisplayName => string.IsNullOrEmpty(Model)
        ? Serial
        : Model.Replace('_', ' ');

    public string StateLabel => State switch
    {
        DeviceConnectionState.Ready        => "Ready",
        DeviceConnectionState.Unauthorized => "Authorization required",
        DeviceConnectionState.Offline      => "Offline",
        DeviceConnectionState.Detected     => "Detected",
        DeviceConnectionState.NoDevice     => "Not connected",
        _                                  => "Unknown"
    };
}
