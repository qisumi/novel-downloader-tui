using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using NovelDownloaderGui.Models;

namespace NovelDownloaderGui.Pages;

public sealed partial class SearchPage : Page
{
    public SearchPage()
    {
        InitializeComponent();
        Loaded += OnLoaded;
    }

    private async void OnLoaded(object sender, RoutedEventArgs e)
    {
        CurrentSourceText.Text = $"当前书源：{AppInstance.SettingsService.CurrentSettings.SourceId}";
        await TryLoadInitialSourcesHintAsync();
    }

    private async Task TryLoadInitialSourcesHintAsync()
    {
        try
        {
            var result = await AppInstance.BackendService.GetSourcesAsync(AppInstance.SettingsService.CurrentSettings);
            var selected = result.Sources.FirstOrDefault(source => source.Id == AppInstance.SettingsService.CurrentSettings.SourceId);
            if (selected is not null)
            {
                CurrentSourceText.Text = $"当前书源：{selected.Name} ({selected.Id})";
            }
        }
        catch
        {
        }
    }

    private async void OnSearchClicked(object sender, RoutedEventArgs e)
    {
        await SearchAsync();
    }

    private async void OnKeywordKeyDown(object sender, KeyRoutedEventArgs e)
    {
        if (e.Key == Windows.System.VirtualKey.Enter)
        {
            e.Handled = true;
            await SearchAsync();
        }
    }

    private async Task SearchAsync()
    {
        try
        {
            SearchStatusBar.IsOpen = false;
            var keywords = KeywordTextBox.Text.Trim();
            if (string.IsNullOrWhiteSpace(keywords))
            {
                ShowStatus(InfoBarSeverity.Warning, "请输入搜索关键词。");
                return;
            }

            var response = await AppInstance.BackendService.SearchBooksAsync(
                AppInstance.SettingsService.CurrentSettings,
                keywords);
            ResultsListView.ItemsSource = response.Books;
            if (response.Books.Count > 0)
            {
                ResultsListView.SelectedIndex = 0;
                ShowStatus(InfoBarSeverity.Success, $"找到 {response.Books.Count} 本书。");
            }
            else
            {
                DetailControl.SetBook(null, false);
                ShowStatus(InfoBarSeverity.Informational, "没有匹配结果。");
            }
        }
        catch (Exception ex)
        {
            ShowStatus(InfoBarSeverity.Error, ex.Message);
        }
    }

    private void OnResultsSelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        DetailControl.SetBook(ResultsListView.SelectedItem as BookRecord, false);
    }

    private void OnPageSizeChanged(object sender, SizeChangedEventArgs e)
    {
        var isNarrow = e.NewSize.Width < 1100;

        if (isNarrow)
        {
            if (ContentGrid.RowDefinitions.Count == 1)
            {
                ContentGrid.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
            }

            ContentGrid.ColumnDefinitions[0].Width = new GridLength(1, GridUnitType.Star);
            ContentGrid.ColumnDefinitions[1].Width = new GridLength(0);
            Grid.SetColumn(ResultsPane, 0);
            Grid.SetRow(ResultsPane, 0);
            Grid.SetColumn(DetailControl, 0);
            Grid.SetRow(DetailControl, 1);
        }
        else
        {
            while (ContentGrid.RowDefinitions.Count > 1)
            {
                ContentGrid.RowDefinitions.RemoveAt(ContentGrid.RowDefinitions.Count - 1);
            }

            ContentGrid.ColumnDefinitions[0].Width = new GridLength(2, GridUnitType.Star);
            ContentGrid.ColumnDefinitions[1].Width = new GridLength(3, GridUnitType.Star);
            Grid.SetColumn(ResultsPane, 0);
            Grid.SetRow(ResultsPane, 0);
            Grid.SetColumn(DetailControl, 1);
            Grid.SetRow(DetailControl, 0);
        }
    }

    private void ShowStatus(InfoBarSeverity severity, string message)
    {
        SearchStatusBar.Severity = severity;
        SearchStatusBar.Message = message;
        SearchStatusBar.IsOpen = true;
    }

    private static App AppInstance => (App)Application.Current;
}
