using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using Microsoft.Extensions.Logging;

namespace Mirra.Shell.Services;

/// <summary>
/// Persists per-device user preferences.
/// Files are keyed by SHA-256(deviceSerial) — never raw serials on disk.
/// </summary>
public sealed class PreferencesService
{
    private readonly ILogger<PreferencesService> _logger;
    private readonly string                       _prefsDir;

    public PreferencesService(ILogger<PreferencesService> logger)
    {
        _logger  = logger;
        _prefsDir = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "Mirra", "preferences");
        Directory.CreateDirectory(_prefsDir);
    }

    public DevicePreferences LoadForDevice(string deviceSerial)
    {
        string path = PrefsPath(deviceSerial);
        if (!File.Exists(path)) return new DevicePreferences();

        try
        {
            string json = File.ReadAllText(path, Encoding.UTF8);
            return JsonSerializer.Deserialize<DevicePreferences>(json) ?? new DevicePreferences();
        }
        catch (Exception ex)
        {
            _logger.LogWarning(ex, "Failed to load preferences for device (hash={Hash})", SerialHash(deviceSerial));
            return new DevicePreferences();
        }
    }

    public void SaveForDevice(string deviceSerial, DevicePreferences prefs)
    {
        string path = PrefsPath(deviceSerial);
        try
        {
            string json = JsonSerializer.Serialize(prefs, new JsonSerializerOptions { WriteIndented = true });
            File.WriteAllText(path, json, Encoding.UTF8);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to save preferences for device (hash={Hash})", SerialHash(deviceSerial));
        }
    }

    private string PrefsPath(string serial) =>
        Path.Combine(_prefsDir, SerialHash(serial) + ".json");

    private static string SerialHash(string serial)
    {
        byte[] hash = SHA256.HashData(Encoding.UTF8.GetBytes(serial));
        return Convert.ToHexString(hash)[..16]; // first 16 hex chars
    }
}

public sealed class DevicePreferences
{
    [JsonPropertyName("qualityMode")]
    public string QualityMode { get; set; } = "medium"; // "low" | "medium" | "high"

    [JsonPropertyName("audioSource")]
    public string AudioSource { get; set; } = "output"; // "output" | "microphone" | "none"

    [JsonPropertyName("clipboardSyncEnabled")]
    public bool ClipboardSyncEnabled { get; set; } = false; // off by default (PRD §10.2)

    [JsonPropertyName("windowWidth")]
    public int WindowWidth { get; set; } = 0;

    [JsonPropertyName("windowHeight")]
    public int WindowHeight { get; set; } = 0;

    [JsonPropertyName("autoStartEnabled")]
    public bool AutoStartEnabled { get; set; } = false;
}
