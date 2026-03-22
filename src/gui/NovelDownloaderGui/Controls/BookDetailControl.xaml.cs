using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using NovelDownloaderGui.Models;

namespace NovelDownloaderGui.Controls;

public sealed partial class BookDetailControl : UserControl
{
    private BookRecord? currentBook_;
    private List<TocItemRecord> currentToc_ = [];

    public BookDetailControl()
    {
        InitializeComponent();
        SetBook(null, false);
    }

    public event EventHandler? BookshelfChanged;

    public void SetBook(BookRecord? book, bool showRemove)
    {
        currentBook_ = book;
        currentToc_.Clear();
        TocListView.ItemsSource = null;
        StatusBar.IsOpen = false;

        if (book is null)
        {
            EmptyStateText.Visibility = Visibility.Visible;
            BookTitleText.Text = "未选择书籍";
            BookMetaText.Text = "作者 / 分类 / 字数 / 评分";
            BookAbstractText.Text = string.Empty;
            TocSummaryText.Text = "目录未加载";
            SaveButton.IsEnabled = false;
            RemoveButton.Visibility = Visibility.Collapsed;
            return;
        }

        EmptyStateText.Visibility = Visibility.Collapsed;
        BookTitleText.Text = book.Title;
        BookMetaText.Text = $"{book.Author}  ·  {book.Category}  ·  {book.WordCount}  ·  评分 {book.Score:F1}";
        BookAbstractText.Text = string.IsNullOrWhiteSpace(book.Abstract)
            ? "暂无简介。"
            : book.Abstract;
        TocSummaryText.Text = "目录未加载";
        SaveButton.IsEnabled = true;
        RemoveButton.Visibility = showRemove ? Visibility.Visible : Visibility.Collapsed;
    }

    private async void OnSaveClicked(object sender, RoutedEventArgs e)
    {
        if (currentBook_ is null)
        {
            return;
        }

        await RunAsync(async () =>
        {
            await AppInstance.BackendService.SaveBookAsync(AppInstance.SettingsService.CurrentSettings, currentBook_);
            ShowStatus(InfoBarSeverity.Success, "已加入书架。");
            BookshelfChanged?.Invoke(this, EventArgs.Empty);
        });
    }

    private async void OnRemoveClicked(object sender, RoutedEventArgs e)
    {
        if (currentBook_ is null)
        {
            return;
        }

        await RunAsync(async () =>
        {
            await AppInstance.BackendService.RemoveBookAsync(
                AppInstance.SettingsService.CurrentSettings,
                currentBook_.BookId);
            ShowStatus(InfoBarSeverity.Success, "已从书架移除。");
            BookshelfChanged?.Invoke(this, EventArgs.Empty);
        });
    }

    private async void OnLoadTocClicked(object sender, RoutedEventArgs e)
    {
        await LoadTocAsync();
    }

    private async void OnDownloadClicked(object sender, RoutedEventArgs e)
    {
        if (currentBook_ is null)
        {
            return;
        }

        await RunAsync(async () =>
        {
            if (currentToc_.Count == 0)
            {
                await LoadTocAsync();
            }

            var result = await AppInstance.BackendService.DownloadBookAsync(
                AppInstance.SettingsService.CurrentSettings,
                currentBook_,
                ForceRemoteCheckBox.IsChecked == true);
            ShowStatus(InfoBarSeverity.Success, $"已缓存 {result.CachedChapterCount}/{result.TotalCount} 章。");
        });
    }

    private async void OnExportEpubClicked(object sender, RoutedEventArgs e)
    {
        await ExportAsync("epub");
    }

    private async void OnExportTxtClicked(object sender, RoutedEventArgs e)
    {
        await ExportAsync("txt");
    }

    private async Task LoadTocAsync()
    {
        if (currentBook_ is null)
        {
            return;
        }

        await RunAsync(async () =>
        {
            var result = await AppInstance.BackendService.LoadTocAsync(
                AppInstance.SettingsService.CurrentSettings,
                currentBook_,
                ForceRemoteCheckBox.IsChecked == true);
            currentToc_ = result.Toc;
            TocListView.ItemsSource = currentToc_;
            TocSummaryText.Text = $"共 {result.TocCount} 章，已缓存 {result.CachedChapterCount} 章。";
            ShowStatus(InfoBarSeverity.Success, "目录已加载。");
        });
    }

    private async Task ExportAsync(string format)
    {
        if (currentBook_ is null)
        {
            return;
        }

        await RunAsync(async () =>
        {
            if (currentToc_.Count == 0)
            {
                await LoadTocAsync();
            }

            var start = 0;
            var end = Math.Max(0, currentToc_.Count - 1);

            var result = await AppInstance.BackendService.ExportBookAsync(
                AppInstance.SettingsService.CurrentSettings,
                currentBook_,
                start,
                end,
                format,
                ForceRemoteCheckBox.IsChecked == true);
            ShowStatus(InfoBarSeverity.Success, $"导出完成：{result.OutputPath}");
        });
    }

    private async Task RunAsync(Func<Task> action)
    {
        try
        {
            BusyRing.IsActive = true;
            SaveButton.IsEnabled = false;
            await action();
        }
        catch (Exception ex)
        {
            ShowStatus(InfoBarSeverity.Error, ex.Message);
        }
        finally
        {
            BusyRing.IsActive = false;
            SaveButton.IsEnabled = currentBook_ is not null;
        }
    }

    private void ShowStatus(InfoBarSeverity severity, string message)
    {
        StatusBar.Severity = severity;
        StatusBar.Message = message;
        StatusBar.IsOpen = true;
    }

    private static App AppInstance => (App)Application.Current;
}
