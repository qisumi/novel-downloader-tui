using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using NovelDownloaderGui.Pages;
namespace NovelDownloaderGui
{
    public sealed partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            Title = "小说下载器 GUI";
            NavigateTo("search");
        }

        private void OnNavigationSelectionChanged(
            NavigationView sender,
            NavigationViewSelectionChangedEventArgs args)
        {
            if (args.IsSettingsSelected)
            {
                NavigateTo("settings");
                return;
            }

            if (args.SelectedItemContainer?.Tag is string tag)
            {
                NavigateTo(tag);
            }
        }

        private void NavigateTo(string tag)
        {
            var pageType = tag switch
            {
                "search" => typeof(SearchPage),
                "bookshelf" => typeof(BookshelfPage),
                "settings" => typeof(SettingsPage),
                _ => typeof(SearchPage),
            };

            if (ContentFrame.CurrentSourcePageType != pageType)
            {
                ContentFrame.Navigate(pageType);
            }
        }
    }
}
