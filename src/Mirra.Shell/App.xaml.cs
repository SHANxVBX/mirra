using System.Windows;
using System.Windows.Media;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Mirra.Shell.Services;
using Mirra.Shell.ViewModels;
using Serilog;

namespace Mirra.Shell;

public partial class App : Application
{
    private IHost? _host;

    protected override async void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        // ── Logging ──────────────────────────────────────────────────────────
        Log.Logger = new LoggerConfiguration()
            .MinimumLevel.Debug()
            .WriteTo.File(
                Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
                    "Mirra", "logs", "shell-.log"),
                rollingInterval: RollingInterval.Day,
                retainedFileCountLimit: 5)
            .CreateLogger();

        // ── System theme detection ────────────────────────────────────────────
        ApplySystemTheme();
        SystemParameters.StaticPropertyChanged += (_, _) => ApplySystemTheme();

        // ── DI Host ──────────────────────────────────────────────────────────
        _host = Host.CreateDefaultBuilder()
            .UseSerilog()
            .ConfigureServices(RegisterServices)
            .Build();

        await _host.StartAsync();

        // ── Start UI Thread Services ─────────────────────────────────────────
        _host.Services.GetRequiredService<ClipboardMonitorService>().Start();

        // ── Show main window ─────────────────────────────────────────────────
        var mainWindow = _host.Services.GetRequiredService<MainWindow>();
        MainWindow = mainWindow;
        mainWindow.Show();
    }

    protected override async void OnExit(ExitEventArgs e)
    {
        Log.Information("Mirra Shell shutting down.");

        if (_host is not null)
        {
            await _host.StopAsync(TimeSpan.FromSeconds(5));
            _host.Dispose();
        }

        Log.CloseAndFlush();
        base.OnExit(e);
    }

    private static void RegisterServices(HostBuilderContext ctx, IServiceCollection services)
    {
        // ── Services ─────────────────────────────────────────────────────────
        services.AddSingleton<AdbMonitorService>();
        services.AddSingleton<CoreProcessService>();
        services.AddSingleton<ClipboardMonitorService>();
        services.AddSingleton<IpcClientService>();
        services.AddSingleton<PreferencesService>();
        services.AddSingleton<DiagnosticsService>();
        services.AddSingleton<INavigationService, NavigationService>();

        // ── ViewModels ───────────────────────────────────────────────────────
        services.AddTransient<DeviceListViewModel>();
        services.AddTransient<CastingViewModel>();
        services.AddTransient<OnboardingViewModel>();
        services.AddTransient<DiagnosticsViewModel>();

        // ── Windows ──────────────────────────────────────────────────────────
        services.AddSingleton<MainWindow>();
    }

    private void ApplySystemTheme()
    {
        // Read Windows apps color mode from registry
        bool isDark = IsSystemDarkMode();
        string themeUri = isDark
            ? "Resources/Themes/Dark.xaml"
            : "Resources/Themes/Light.xaml";

        var dict = new ResourceDictionary { Source = new Uri(themeUri, UriKind.Relative) };

        // Replace the first merged dictionary (the theme)
        if (Resources.MergedDictionaries.Count > 0)
            Resources.MergedDictionaries[0] = dict;
    }

    private static bool IsSystemDarkMode()
    {
        try
        {
            using var key = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(
                @"Software\Microsoft\Windows\CurrentVersion\Themes\Personalize");
            var value = key?.GetValue("AppsUseLightTheme");
            return value is int i && i == 0;
        }
        catch { return true; } // default to dark
    }
}
