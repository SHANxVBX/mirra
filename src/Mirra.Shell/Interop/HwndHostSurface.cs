using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Interop;

namespace Mirra.Shell.Interop;

/// <summary>
/// Embeds the Casting.Core SDL3 window (identified by its HWND) inside WPF
/// using HwndHost. Only activated after Core reports CoreHwnd via IPC.
/// </summary>
public sealed class HwndHostSurface : HwndHost
{
    private IntPtr _coreHwnd;
    private IntPtr _hostHwnd;
    private bool   _attached = false;

    [DllImport("user32.dll")] private static extern IntPtr SetParent(IntPtr hWnd, IntPtr hWndParent);
    [DllImport("user32.dll")] private static extern bool  MoveWindow(IntPtr hWnd, int x, int y, int w, int h, bool repaint);
    [DllImport("user32.dll")] private static extern int   GetWindowLong(IntPtr hWnd, int nIndex);
    [DllImport("user32.dll")] private static extern int   SetWindowLong(IntPtr hWnd, int nIndex, int dwNewLong);

    private const int GWL_STYLE      = -16;
    private const int WS_CHILD       = 0x40000000;
    private const int WS_POPUP       = unchecked((int)0x80000000);
    private const int WS_CAPTION     = 0x00C00000;
    private const int WS_THICKFRAME  = 0x00040000;

    protected override HandleRef BuildWindowCore(HandleRef hwndParent)
    {
        _hostHwnd = hwndParent.Handle;

        // Create a transparent host HWND that Core's window will be reparented into
        var hwnd = CreateHostWindow(hwndParent.Handle);
        return new HandleRef(this, hwnd);
    }

    protected override void DestroyWindowCore(HandleRef hwnd)
    {
        DetachCore();
    }

    public event Action<string, float, float>? PointerEvent;
    public event Action<int, int, int>?        KeyEvent;
    public event Action<float, float, float, float>? ScrollEvent;

    private delegate IntPtr WndProcDelegate(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
    private WndProcDelegate? _coreWndProc;
    private IntPtr           _oldWndProc;

    [DllImport("user32.dll", EntryPoint = "SetWindowLongPtr")]
    private static extern IntPtr SetWindowLongPtr64(IntPtr hWnd, int nIndex, IntPtr dwNewLong);

    [DllImport("user32.dll", EntryPoint = "SetWindowLong")]
    private static extern IntPtr SetWindowLong32(IntPtr hWnd, int nIndex, IntPtr dwNewLong);

    private static IntPtr SetWindowLongPtr(IntPtr hWnd, int nIndex, IntPtr dwNewLong)
    {
        if (IntPtr.Size == 8)
            return SetWindowLongPtr64(hWnd, nIndex, dwNewLong);
        else
            return SetWindowLong32(hWnd, nIndex, dwNewLong);
    }

    [DllImport("user32.dll")]
    private static extern IntPtr CallWindowProc(IntPtr lpPrevWndFunc, IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);

    public void AttachCore(IntPtr coreHwnd)
    {
        if (coreHwnd == IntPtr.Zero || _attached) return;
        _coreHwnd = coreHwnd;

        // Strip popup/caption styles; make it a child window
        int style = GetWindowLong(_coreHwnd, GWL_STYLE);
        style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME);
        style |= WS_CHILD;
        SetWindowLong(_coreHwnd, GWL_STYLE, style);

        SetParent(_coreHwnd, Handle);

        // Subclass to intercept inputs
        _coreWndProc = new WndProcDelegate(CoreWndProc);
        _oldWndProc = SetWindowLongPtr(_coreHwnd, -4 /* GWLP_WNDPROC */, Marshal.GetFunctionPointerForDelegate(_coreWndProc));

        FitCoreWindow();
        _attached = true;
    }

    public void DetachCore()
    {
        if (!_attached || _coreHwnd == IntPtr.Zero) return;
        
        if (_oldWndProc != IntPtr.Zero)
            SetWindowLongPtr(_coreHwnd, -4, _oldWndProc);

        SetParent(_coreHwnd, IntPtr.Zero);
        _attached = false;
    }

    private IntPtr CoreWndProc(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam)
    {
        int x = (short)((uint)lParam & 0xFFFF);
        int y = (short)((uint)lParam >> 16);
        float nx = (float)x / (float)RenderSize.Width;
        float ny = (float)y / (float)RenderSize.Height;

        switch (msg)
        {
            case 0x0201: // WM_LBUTTONDOWN
                PointerEvent?.Invoke("touch_down", nx, ny);
                break;
            case 0x0202: // WM_LBUTTONUP
                PointerEvent?.Invoke("touch_up", nx, ny);
                break;
            case 0x0200: // WM_MOUSEMOVE
                if ((wParam.ToInt32() & 0x0001) != 0) // MK_LBUTTON
                    PointerEvent?.Invoke("touch_move", nx, ny);
                break;
            case 0x020A: // WM_MOUSEWHEEL
                int delta = (short)((wParam.ToInt64() >> 16) & 0xFFFF);
                ScrollEvent?.Invoke(nx, ny, 0, (float)delta / 120.0f);
                break;
            case 0x0100: // WM_KEYDOWN
            case 0x0101: // WM_KEYUP
                int action = msg == 0x0100 ? 0 : 1;
                KeyEvent?.Invoke(action, wParam.ToInt32(), 0);
                break;
        }
        return CallWindowProc(_oldWndProc, hWnd, msg, wParam, lParam);
    }

    /// <summary>Called when the WPF host size changes — relay to Core SDL3 window.</summary>
    public void FitCoreWindow()
    {
        if (!_attached || _coreHwnd == IntPtr.Zero) return;

        var size = RenderSize;
        MoveWindow(_coreHwnd, 0, 0,
            (int)size.Width, (int)size.Height, repaint: true);
    }

    protected override void OnRenderSizeChanged(SizeChangedInfo info)
    {
        base.OnRenderSizeChanged(info);
        FitCoreWindow();
    }

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern IntPtr CreateWindowEx(int dwExStyle, string lpszClassName, string lpszWindowName, int style, int x, int y, int width, int height, IntPtr hwndParent, IntPtr hMenu, IntPtr hInst, IntPtr pvParam);

    private const int WS_VISIBLE     = 0x10000000;
    private const int WS_CLIPCHILDREN = 0x02000000;

    private static IntPtr CreateHostWindow(IntPtr parent)
    {
        // Create a minimal static child window to host the core HWND
        return CreateWindowEx(0, "Static", "", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN, 0, 0, 0, 0, parent, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
    }
}
