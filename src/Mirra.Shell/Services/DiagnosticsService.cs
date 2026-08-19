using System.IO.Compression;
using System.Text.RegularExpressions;
using Microsoft.Extensions.Logging;

namespace Mirra.Shell.Services;

/// <summary>
/// Collects structured logs and exports a redacted ZIP diagnostic package.
/// Strips: raw serials, usernames, clipboard, file contents, media.
/// </summary>
public sealed class DiagnosticsService
{
    private readonly ILogger<DiagnosticsService> _logger;
    private readonly AdbMonitorService _adbMonitor;

    private static readonly string LogDir = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
        "Mirra", "logs");

    public DiagnosticsService(ILogger<DiagnosticsService> logger, AdbMonitorService adbMonitor)
    {
        _logger = logger;
        _adbMonitor = adbMonitor;
    }

    /// <summary>
    /// Returns a preview of what categories will be included in the export ZIP.
    /// </summary>
    public IReadOnlyList<DiagnosticCategory> GetExportPreview()
    {
        return new[]
        {
            new DiagnosticCategory("Shell logs",     GetShellLogs().Count > 0,   "Mirra Shell structured log files (redacted)"),
            new DiagnosticCategory("Core logs",      GetCoreLogs().Count > 0,    "Casting.Core diagnostic log files (redacted)"),
            new DiagnosticCategory("Version manifest", true, "Pinned dependency versions"),
            new DiagnosticCategory("Device summary", true,                        "Device capability and ADB state summary"),
        };
    }

    /// <summary>
    /// Exports a redacted ZIP to the given path.
    /// </summary>
    public async Task ExportAsync(string outputPath, CancellationToken ct = default)
    {
        _logger.LogInformation("Starting diagnostic export to {Path}", outputPath);

        using var zip = ZipFile.Open(outputPath, ZipArchiveMode.Create);

        // Shell logs
        foreach (var logFile in GetShellLogs())
        {
            await AddRedactedFileAsync(zip, logFile, "shell-logs/" + Path.GetFileName(logFile), ct);
        }

        // Core logs
        foreach (var logFile in GetCoreLogs())
        {
            await AddRedactedFileAsync(zip, logFile, "core-logs/" + Path.GetFileName(logFile), ct);
        }

        // Version manifest
        string manifestPath = Path.Combine(AppContext.BaseDirectory, "release-manifest.json");
        if (File.Exists(manifestPath))
            zip.CreateEntryFromFile(manifestPath, "release-manifest.json");

        // Device capability summary (generated at runtime)
        string summary = await GenerateDeviceSummaryAsync(ct);
        var entry = zip.CreateEntry("device-summary.txt");
        await using var writer = new StreamWriter(entry.Open());
        await writer.WriteAsync(summary);

        _logger.LogInformation("Diagnostic export complete: {Path}", outputPath);
    }

    private async Task AddRedactedFileAsync(ZipArchive zip, string sourcePath, string entryName, CancellationToken ct)
    {
        try
        {
            string content = await File.ReadAllTextAsync(sourcePath, ct);
            string redacted = Redact(content);
            var entry = zip.CreateEntry(entryName);
            await using var writer = new StreamWriter(entry.Open());
            await writer.WriteAsync(redacted);
        }
        catch (Exception ex)
        {
            _logger.LogWarning(ex, "Failed to add {File} to diagnostic ZIP", sourcePath);
        }
    }

    /// <summary>
    /// Redacts sensitive patterns from log content:
    /// - Device serial numbers (replaced with [SERIAL_REDACTED])
    /// - Local usernames from paths
    /// - Clipboard text
    /// </summary>
    private static string Redact(string content)
    {
        // Windows username in paths: C:\Users\{username}\
        content = Regex.Replace(content,
            @"C:\\Users\\[^\\]+\\",
            @"C:\Users\[USER]\",
            RegexOptions.IgnoreCase);

        // ADB serial-like patterns (e.g., R3CN901XXXXX or emulator-5554)
        content = Regex.Replace(content,
            @"\b([A-Z0-9]{8,16}|emulator-\d{4})\b",
            "[SERIAL_REDACTED]");

        // Clipboard field values in JSON logs
        content = Regex.Replace(content,
            @"""clipboard""\s*:\s*""[^""]*""",
            @"""clipboard"":""[REDACTED]""");

        return content;
    }

    private Task<string> GenerateDeviceSummaryAsync(CancellationToken ct)
    {
        var devices = _adbMonitor.CurrentDevices;
        var summary = new System.Text.StringBuilder();
        summary.AppendLine("Device capability summary: (generated at export time)");
        foreach (var device in devices)
        {
            summary.AppendLine($"- Serial: [SERIAL_REDACTED]");
            summary.AppendLine($"  Model: {device.Model}");
            summary.AppendLine($"  State: {device.State}");
        }
        return Task.FromResult(summary.ToString());
    }

    private static List<string> GetShellLogs()
    {
        if (!Directory.Exists(LogDir)) return new();
        return Directory.GetFiles(LogDir, "shell-*.log").ToList();
    }

    private static List<string> GetCoreLogs()
    {
        if (!Directory.Exists(LogDir)) return new();
        return Directory.GetFiles(LogDir, "core-*.log").ToList();
    }
}

public sealed record DiagnosticCategory(string Name, bool HasData, string Description);
