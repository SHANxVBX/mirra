using System.Windows;
using System.Windows.Input;
using CommunityToolkit.Mvvm.ComponentModel;
using Mirra.Shell.Services;
using Mirra.Shell.ViewModels;

namespace Mirra.Shell;

public sealed partial class MainWindow : Window
{
    private readonly INavigationService _navigation;

    public ObservableObject? CurrentViewModel
    {
        get => (ObservableObject?)GetValue(CurrentViewModelProperty);
        set => SetValue(CurrentViewModelProperty, value);
    }

    public static readonly DependencyProperty CurrentViewModelProperty =
        DependencyProperty.Register(nameof(CurrentViewModel), typeof(ObservableObject), typeof(MainWindow));

    public MainWindow(INavigationService navigation, DeviceListViewModel deviceListVm)
    {
        InitializeComponent();
        DataContext = this;

        _navigation = navigation;
        _navigation.ViewModelChanged += (_, vm) =>
        {
            Dispatcher.Invoke(() => CurrentViewModel = vm);
        };

        // Start on device list
        CurrentViewModel = deviceListVm;
    }

    private void TitleBar_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ClickCount == 2)
        {
            WindowState = WindowState == WindowState.Maximized
                ? WindowState.Normal
                : WindowState.Maximized;
        }
        else
        {
            DragMove();
        }
    }

    private void MinimizeButton_Click(object sender, RoutedEventArgs e) =>
        WindowState = WindowState.Minimized;

    private void MaximizeButton_Click(object sender, RoutedEventArgs e) =>
        WindowState = WindowState == WindowState.Maximized
            ? WindowState.Normal
            : WindowState.Maximized;

    private void CloseButton_Click(object sender, RoutedEventArgs e) => Close();
}
