using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Mirra.Shell.Services;
using Microsoft.Extensions.Logging;

namespace Mirra.Shell.ViewModels;

public sealed partial class DiagnosticsViewModel : ObservableObject
{
    private readonly DiagnosticsService  _diagService;
    private readonly INavigationService   _navigation;
    private readonly ILogger<DiagnosticsViewModel> _logger;

    [ObservableProperty]
    private IReadOnlyList<DiagnosticCategory> _categories = Array.Empty<DiagnosticCategory>();

    [ObservableProperty]
    private bool _isExporting = false;

    [ObservableProperty]
    private string? _exportStatusMessage;

    public DiagnosticsViewModel(
        DiagnosticsService diagService,
        INavigationService navigation,
        ILogger<DiagnosticsViewModel> logger)
    {
        _diagService = diagService;
        _navigation  = navigation;
        _logger      = logger;

        Categories = _diagService.GetExportPreview();
    }

    [RelayCommand]
    private async Task ExportZipAsync()
    {
        IsExporting = true;
        ExportStatusMessage = "Exporting redacted diagnostic archive...";

        try
        {
            var desktop = Environment.GetFolderPath(Environment.SpecialFolder.Desktop);
            var timestamp = DateTime.Now.ToString("yyyyMMdd_HHmmss");
            var outPath = Path.Combine(desktop, $"Mirra_Diagnostics_{timestamp}.zip");

            await _diagService.ExportAsync(outPath);
            ExportStatusMessage = $"Saved to Desktop: Mirra_Diagnostics_{timestamp}.zip";
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to export diagnostic ZIP");
            ExportStatusMessage = $"Export failed: {ex.Message}";
        }
        finally
        {
            IsExporting = false;
        }
    }

    [RelayCommand]
    private void Back()
    {
        _navigation.NavigateTo<DeviceListViewModel>();
    }
}
