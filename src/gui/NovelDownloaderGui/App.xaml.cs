using Microsoft.UI.Xaml;
using NovelDownloaderGui.Services;

namespace NovelDownloaderGui
{
    public partial class App : Application
    {
        public App()
        {
            Environment.SetEnvironmentVariable(
                "MICROSOFT_WINDOWSAPPRUNTIME_BASE_DIRECTORY",
                AppContext.BaseDirectory);
            InitializeComponent();
            SettingsService = new AppSettingsService();
            BackendService = new BackendService();
        }

        public AppSettingsService SettingsService { get; }
        public BackendService     BackendService { get; }

        protected override void OnLaunched(LaunchActivatedEventArgs args)
        {
            m_window = new MainWindow();
            m_window.Activate();
        }

        private Window? m_window;
    }
}
