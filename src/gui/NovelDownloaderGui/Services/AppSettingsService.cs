using System.Text.Json;
using NovelDownloaderGui.Helpers;
using NovelDownloaderGui.Models;

namespace NovelDownloaderGui.Services;

public sealed class AppSettingsService
{
    private static readonly JsonSerializerOptions JsonOptions = new(JsonSerializerDefaults.Web)
    {
        WriteIndented = true,
    };

    private readonly string settingsFilePath_;

    public AppSettingsService()
    {
        var settingsDir = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "NovelDownloaderGui");
        Directory.CreateDirectory(settingsDir);

        settingsFilePath_ = Path.Combine(settingsDir, "settings.json");
        CurrentSettings = Load();
    }

    public AppSettings CurrentSettings { get; private set; }

    public async Task SaveAsync(AppSettings settings)
    {
        CurrentSettings = settings;
        Directory.CreateDirectory(Path.GetDirectoryName(settingsFilePath_)!);
        var json = JsonSerializer.Serialize(settings, JsonOptions);
        await File.WriteAllTextAsync(settingsFilePath_, json);
    }

    private AppSettings Load()
    {
        try
        {
            if (File.Exists(settingsFilePath_))
            {
                var content = File.ReadAllText(settingsFilePath_);
                var settings = JsonSerializer.Deserialize<AppSettings>(content, JsonOptions);
                if (settings is not null)
                {
                    Normalize(settings);
                    return settings;
                }
            }
        }
        catch
        {
        }

        var defaults = CreateDefaults();
        Normalize(defaults);
        return defaults;
    }

    private static AppSettings CreateDefaults()
    {
        var repoRoot = PathLocator.FindRepositoryRoot();
        return new AppSettings
        {
            RepositoryRoot = repoRoot,
            PluginDirectory = Path.Combine(repoRoot, "plugins"),
            DatabasePath = Path.Combine(repoRoot, "novel.db"),
            OutputDirectory = Path.Combine(repoRoot, "exports"),
            SourceId = "fanqie",
        };
    }

    private static void Normalize(AppSettings settings)
    {
        if (string.IsNullOrWhiteSpace(settings.RepositoryRoot))
        {
            settings.RepositoryRoot = PathLocator.FindRepositoryRoot();
        }

        if (string.IsNullOrWhiteSpace(settings.PluginDirectory))
        {
            settings.PluginDirectory = Path.Combine(settings.RepositoryRoot, "plugins");
        }

        if (string.IsNullOrWhiteSpace(settings.DatabasePath))
        {
            settings.DatabasePath = Path.Combine(settings.RepositoryRoot, "novel.db");
        }

        if (string.IsNullOrWhiteSpace(settings.OutputDirectory))
        {
            settings.OutputDirectory = Path.Combine(settings.RepositoryRoot, "exports");
        }

        if (string.IsNullOrWhiteSpace(settings.SourceId))
        {
            settings.SourceId = "fanqie";
        }
    }
}
