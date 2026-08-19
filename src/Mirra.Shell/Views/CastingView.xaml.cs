using System.Windows.Controls;
using Mirra.Shell.ViewModels;

namespace Mirra.Shell.Views;

public partial class CastingView : UserControl
{
    public CastingView()
    {
        InitializeComponent();
        DataContextChanged += OnDataContextChanged;
    }

    private void OnDataContextChanged(object sender, System.Windows.DependencyPropertyChangedEventArgs e)
    {
        if (e.NewValue is CastingViewModel vm)
        {
            vm.CoreHwndReady += (_, hwnd) =>
            {
                Dispatcher.Invoke(() =>
                {
                    VideoSurfaceHost.AttachCore((IntPtr)hwnd);
                });
            };

            // Wire input events
            VideoSurfaceHost.PointerEvent += (eventType, x, y) =>
            {
                _ = vm.SendTouchAsync(eventType, x, y);
            };
            VideoSurfaceHost.KeyEvent += (action, keyCode, metaState) =>
            {
                _ = vm.SendKeyAsync(action, keyCode, metaState);
            };
            VideoSurfaceHost.ScrollEvent += (x, y, scrollX, scrollY) =>
            {
                _ = vm.SendScrollAsync(x, y, scrollX, scrollY);
            };

            // If HWND was already reported before view loaded
            if (vm.CoreHwnd != 0)
            {
                VideoSurfaceHost.AttachCore((IntPtr)vm.CoreHwnd);
            }
        }
    }
}
