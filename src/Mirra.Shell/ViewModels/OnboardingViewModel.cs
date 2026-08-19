using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Mirra.Shell.Services;

namespace Mirra.Shell.ViewModels;

public sealed partial class OnboardingViewModel : ObservableObject
{
    private readonly INavigationService _navigation;

    [ObservableProperty]
    private int _currentStep = 1; // 1, 2, or 3

    public OnboardingViewModel(INavigationService navigation)
    {
        _navigation = navigation;
    }

    [RelayCommand]
    private void NextStep()
    {
        if (CurrentStep < 3)
        {
            CurrentStep++;
        }
        else
        {
            _navigation.NavigateTo<DeviceListViewModel>();
        }
    }

    [RelayCommand]
    private void PreviousStep()
    {
        if (CurrentStep > 1)
        {
            CurrentStep--;
        }
    }

    [RelayCommand]
    private void Skip()
    {
        _navigation.NavigateTo<DeviceListViewModel>();
    }
}
