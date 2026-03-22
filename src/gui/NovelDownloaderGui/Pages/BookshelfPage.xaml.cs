using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using NovelDownloaderGui.Models;

namespace NovelDownloaderGui.Pages;

public sealed partial class BookshelfPage : Page
{
    public BookshelfPage()
    {
        InitializeComponent();
        Loaded += OnLoaded;
    }

    private async void OnLoaded(object sender, RoutedEventArgs e)
    {
        BookshelfHintText.Text = $"当前书源：{AppInstance.SettingsService.CurrentSettings.SourceId}";
        await RefreshAsync();
    }

    private async void OnRefreshClicked(object sender, RoutedEventArgs e)
    {
        await RefreshAsync();
    }

    private async Task RefreshAsync()
    {
        try
        {
            var response = await AppInstance.BackendService.GetBookshelfAsync(
                AppInstance.SettingsService.CurrentSettings);
            BookshelfListView.ItemsSource = response.Books;
            if (response.Books.Count > 0)
            {
                BookshelfListView.SelectedIndex = 0;
                ShowStatus(InfoBarSeverity.Success, $"已加载 {response.Books.Count} 本书。");
            }
            else
            {
                DetailControl.SetBook(null, true);
                ShowStatus(InfoBarSeverity.Informational, "当前书架为空。");
            }
        }
        catch (Exception ex)
        {
            ShowStatus(InfoBarSeverity.Error, ex.Message);
        }
    }

    private void OnBooksSelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        DetailControl.SetBook(BookshelfListView.SelectedItem as BookRecord, true);
    }

    private async void OnBookshelfChanged(object sender, EventArgs e)
    {
        await RefreshAsync();
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
            Grid.SetColumn(BooksPane, 0);
            Grid.SetRow(BooksPane, 0);
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
            Grid.SetColumn(BooksPane, 0);
            Grid.SetRow(BooksPane, 0);
            Grid.SetColumn(DetailControl, 1);
            Grid.SetRow(DetailControl, 0);
        }
    }

    private void ShowStatus(InfoBarSeverity severity, string message)
    {
        BookshelfStatusBar.Severity = severity;
        BookshelfStatusBar.Message = message;
        BookshelfStatusBar.IsOpen = true;
    }

    private static App AppInstance => (App)Application.Current;
}
