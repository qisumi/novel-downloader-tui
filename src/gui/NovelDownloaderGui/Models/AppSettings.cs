namespace NovelDownloaderGui.Models;

public sealed class AppSettings
{
    public string RepositoryRoot { get; set; } = string.Empty;
    public string PluginDirectory { get; set; } = string.Empty;
    public string DatabasePath { get; set; } = string.Empty;
    public string OutputDirectory { get; set; } = string.Empty;
    public string SourceId { get; set; } = "fanqie";
}
