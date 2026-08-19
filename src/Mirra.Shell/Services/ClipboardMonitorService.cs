using System;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;
using Microsoft.Extensions.Logging;

namespace Mirra.Shell.Services;

public sealed class ClipboardMonitorService : IDisposable
{
    private readonly ILogger<ClipboardMonitorService> _logger;
    private HwndSource? _hwndSource;
    private string _lastSyncedText = string.Empty;
    private bool _isSettingClipboard = false;

    public event EventHandler<string>? ClipboardChanged;

    private const int WM_CLIPBOARDUPDATE = 0x031D;

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool AddClipboardFormatListener(IntPtr hwnd);

    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool RemoveClipboardFormatListener(IntPtr hwnd);

    public ClipboardMonitorService(ILogger<ClipboardMonitorService> logger)
    {
        _logger = logger;
    }

    public void Start()
    {
        if (_hwndSource != null) return;

        Application.Current.Dispatcher.Invoke(() =>
        {
            var parameters = new HwndSourceParameters("ClipboardMonitor")
            {
                WindowStyle = 0,
                Width = 0,
                Height = 0,
                ExtendedWindowStyle = 0,
                ParentWindow = (IntPtr)(-3) // HWND_MESSAGE
            };

            _hwndSource = new HwndSource(parameters);
            _hwndSource.AddHook(WndProc);
            if (!AddClipboardFormatListener(_hwndSource.Handle))
            {
                _logger.LogError("Failed to add clipboard format listener. Error: {Error}", Marshal.GetLastWin32Error());
            }
            else
            {
                _logger.LogInformation("Clipboard monitor started.");
            }
        });
    }

    private IntPtr WndProc(IntPtr hwnd, int msg, IntPtr wParam, IntPtr lParam, ref bool handled)
    {
        if (msg == WM_CLIPBOARDUPDATE)
        {
            if (_isSettingClipboard) return IntPtr.Zero;

            Application.Current.Dispatcher.InvokeAsync(() =>
            {
                try
                {
                    if (Clipboard.ContainsText())
                    {
                        string text = Clipboard.GetText();
                        if (text != _lastSyncedText)
                        {
                            _lastSyncedText = text;
                            ClipboardChanged?.Invoke(this, text);
                        }
                    }
                }
                catch (Exception ex)
                {
                    _logger.LogWarning(ex, "Failed to read clipboard text.");
                }
            });
        }
        return IntPtr.Zero;
    }

    public void SetClipboard(string text)
    {
        if (text == _lastSyncedText) return;

        _lastSyncedText = text;
        Application.Current.Dispatcher.InvokeAsync(() =>
        {
            _isSettingClipboard = true;
            try
            {
                Clipboard.SetText(text);
                _logger.LogInformation("Clipboard text updated from IPC.");
            }
            catch (Exception ex)
            {
                _logger.LogWarning(ex, "Failed to set clipboard text.");
            }
            finally
            {
                _isSettingClipboard = false;
            }
        });
    }

    public void Dispose()
    {
        if (_hwndSource != null)
        {
            Application.Current.Dispatcher.Invoke(() =>
            {
                RemoveClipboardFormatListener(_hwndSource.Handle);
                _hwndSource.RemoveHook(WndProc);
                _hwndSource.Dispose();
            });
            _hwndSource = null;
        }
    }
}
