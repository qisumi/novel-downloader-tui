using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using NovelDownloaderGui.Models;

namespace NovelDownloaderGui.Pages;

public sealed partial class SettingsPage : Page
{
    public SettingsPage()
    {
        InitializeComponent();
        Loaded += OnLoaded;
    }

    private async void OnLoaded(object sender, RoutedEventArgs e)
    {
        LoadSettingsIntoForm();
        await LoadSourcesAsync();
    }

    private void LoadSettingsIntoForm()
    {
        var settings = AppInstance.SettingsService.CurrentSettings;
        RepositoryRootTextBox.Text = settings.RepositoryRoot;
        PluginDirectoryTextBox.Text = settings.PluginDirectory;
        DatabasePathTextBox.Text = settings.DatabasePath;
        OutputDirectoryTextBox.Text = settings.OutputDirectory;
    }

    private async Task LoadSourcesAsync()
    {
        try
        {
            var response = await AppInstance.BackendService.GetSourcesAsync(AppInstance.SettingsService.CurrentSettings);
            SourceComboBox.ItemsSource = response.Sources;
            SourceComboBox.SelectedValue = AppInstance.SettingsService.CurrentSettings.SourceId;
            ShowStatus(InfoBarSeverity.Success, $"已加载 {response.Sources.Count} 个书源。");
        }
        catch (Exception ex)
        {
            ShowStatus(InfoBarSeverity.Warning, ex.Message);
        }
    }

    private async void OnReloadSourcesClicked(object sender, RoutedEventArgs e)
    {
        await SaveCurrentFormAsync();
        await LoadSourcesAsync();
    }

    private async void OnSaveClicked(object sender, RoutedEventArgs e)
    {
        await SaveCurrentFormAsync();
        ShowStatus(InfoBarSeverity.Success, "设置已保存。");
    }

    private async Task SaveCurrentFormAsync()
    {
        var settings = new AppSettings
        {
            RepositoryRoot = RepositoryRootTextBox.Text.Trim(),
            PluginDirectory = PluginDirectoryTextBox.Text.Trim(),
            DatabasePath = DatabasePathTextBox.Text.Trim(),
            OutputDirectory = OutputDirectoryTextBox.Text.Trim(),
            SourceId = SourceComboBox.SelectedValue as string ?? AppInstance.SettingsService.CurrentSettings.SourceId,
        };

        await AppInstance.SettingsService.SaveAsync(settings);
    }

    private void ShowStatus(InfoBarSeverity severity, string message)
    {
        SettingsStatusBar.Severity = severity;
        SettingsStatusBar.Message = message;
        SettingsStatusBar.IsOpen = true;
    }

    private static App AppInstance => (App)Application.Current;
}
