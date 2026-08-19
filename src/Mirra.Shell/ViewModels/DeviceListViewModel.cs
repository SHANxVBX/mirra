using System.Collections.ObjectModel;
using System.Reactive.Linq;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Mirra.Shell.Models;
using Mirra.Shell.Services;
using Microsoft.Extensions.Logging;

namespace Mirra.Shell.ViewModels;

public sealed partial class DeviceListViewModel : ObservableObject, IDisposable
{
    private readonly AdbMonitorService   _adbMonitor;
    private readonly CoreProcessService  _coreService;
    private readonly PreferencesService  _prefsService;
    private readonly INavigationService  _navigation;
    private readonly ILogger<DeviceListViewModel> _logger;
    private IDisposable?                  _subscription;

    [ObservableProperty]
    private ObservableCollection<DeviceInfo> _devices = [];

    [ObservableProperty]
    private DeviceInfo? _selectedDevice;

    [ObservableProperty]
    [NotifyCanExecuteChangedFor(nameof(StartCastingCommand))]
    private bool _isStarting = false;

    [ObservableProperty]
    private string _statusMessage = "Waiting for device...";

    [ObservableProperty]
    private bool _hasNoDevices = true;

    [ObservableProperty]
    private string _qualityMode = "high"; // "low", "medium", "high"

    [ObservableProperty]
    private bool _audioForwarding = true;

    [ObservableProperty]
    private bool _clipboardSync = false;

    public bool CanStartCasting =>
        SelectedDevice?.State == DeviceConnectionState.Ready && !IsStarting;

    public DeviceListViewModel(
        AdbMonitorService adbMonitor,
        CoreProcessService coreService,
        PreferencesService prefsService,
        INavigationService navigation,
        ILogger<DeviceListViewModel> logger)
    {
        _adbMonitor   = adbMonitor;
        _coreService  = coreService;
        _prefsService = prefsService;
        _navigation   = navigation;
        _logger       = logger;

        _adbMonitor.Start();
        _subscription = _adbMonitor.Devices
            .ObserveOn(System.Reactive.Concurrency.DispatcherScheduler.Current)
            .Subscribe(OnDevicesChanged);
    }

    private void OnDevicesChanged(IReadOnlyList<DeviceInfo> devices)
    {
        Devices.Clear();
        foreach (var d in devices) Devices.Add(d);
        
        HasNoDevices = devices.Count == 0;

        // Auto-select if exactly one ready device
        var ready = devices.Where(d => d.State == DeviceConnectionState.Ready).ToList();
        if (ready.Count == 1 && SelectedDevice is null)
        {
            SelectedDevice = ready[0];
        }

        StatusMessage = devices.Count == 0
            ? "No device detected. Connect via USB with USB Debugging enabled."
            : $"{devices.Count} device(s) connected.";
    }

    [RelayCommand(CanExecute = nameof(CanStartCasting))]
    private async Task StartCastingAsync()
    {
        if (SelectedDevice is null) return;

        IsStarting    = true;
        StatusMessage = $"Starting session for {SelectedDevice.DisplayName}...";

        try
        {
            // Save preferences
            _prefsService.SaveForDevice(SelectedDevice.Serial, new DevicePreferences
            {
                QualityMode = QualityMode,
                AudioSource = AudioForwarding ? "output" : "none",
                ClipboardSyncEnabled = ClipboardSync
            });

            await _coreService.StartAsync(SelectedDevice.Serial);
            _navigation.NavigateTo<CastingViewModel>();
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to start casting session");
            StatusMessage = $"Failed to start: {ex.Message}";
        }
        finally
        {
            IsStarting = false;
        }
    }

    [RelayCommand]
    private void OpenOnboarding()
    {
        _navigation.NavigateTo<OnboardingViewModel>();
    }

    [RelayCommand]
    private void OpenDiagnostics()
    {
        _navigation.NavigateTo<DiagnosticsViewModel>();
    }

    partial void OnSelectedDeviceChanged(DeviceInfo? value)
    {
        StartCastingCommand.NotifyCanExecuteChanged();
        if (value is not null)
        {
            var prefs = _prefsService.LoadForDevice(value.Serial);
            QualityMode = prefs.QualityMode;
            AudioForwarding = prefs.AudioSource != "none";
            ClipboardSync = prefs.ClipboardSyncEnabled;
        }
    }

    public void Dispose()
    {
        _subscription?.Dispose();
        _adbMonitor.Stop();
    }
}
