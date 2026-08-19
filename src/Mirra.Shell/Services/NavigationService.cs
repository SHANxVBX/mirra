using System.Windows;
using System.Windows.Controls;
using CommunityToolkit.Mvvm.ComponentModel;

namespace Mirra.Shell.Services;

public interface INavigationService
{
    void NavigateTo<TViewModel>() where TViewModel : ObservableObject;
    event EventHandler<ObservableObject>? ViewModelChanged;
}

public sealed class NavigationService : INavigationService
{
    private readonly IServiceProvider _services;
    public event EventHandler<ObservableObject>? ViewModelChanged;

    public NavigationService(IServiceProvider services)
    {
        _services = services;
    }

    public void NavigateTo<TViewModel>() where TViewModel : ObservableObject
    {
        var vm = _services.GetService(typeof(TViewModel)) as ObservableObject
            ?? throw new InvalidOperationException($"No registered VM: {typeof(TViewModel).Name}");
        ViewModelChanged?.Invoke(this, vm);
    }
}
