namespace NovelDownloaderGui.Helpers;

public static class PathLocator
{
    public static string FindBridgeLibrary()
    {
        foreach (var candidate in CandidateBridgePaths())
        {
            if (File.Exists(candidate))
            {
                return candidate;
            }
        }

        return string.Empty;
    }

    public static string FindRepositoryRoot()
    {
        foreach (var start in CandidateRoots())
        {
            var current = new DirectoryInfo(start);
            while (current is not null)
            {
                var cmakePath = Path.Combine(current.FullName, "CMakeLists.txt");
                var pluginsPath = Path.Combine(current.FullName, "plugins");
                if (File.Exists(cmakePath) && Directory.Exists(pluginsPath))
                {
                    return current.FullName;
                }

                var bridgePath = Path.Combine(current.FullName, "novel-gui-bridge.dll");
                if (File.Exists(bridgePath) && Directory.Exists(pluginsPath))
                {
                    return current.FullName;
                }

                current = current.Parent;
            }
        }

        return AppContext.BaseDirectory;
    }

    private static IEnumerable<string> CandidateBridgePaths()
    {
        yield return Path.Combine(AppContext.BaseDirectory, "novel-gui-bridge.dll");

        var processPath = Environment.ProcessPath;
        if (!string.IsNullOrWhiteSpace(processPath))
        {
            var processDir = Path.GetDirectoryName(processPath);
            if (!string.IsNullOrWhiteSpace(processDir))
            {
                yield return Path.Combine(processDir, "novel-gui-bridge.dll");
            }
        }

        var repoRoot = FindRepositoryRoot();
        var buildRoot = Path.Combine(repoRoot, "build");
        if (!Directory.Exists(buildRoot))
        {
            yield break;
        }

        foreach (var path in Directory
                     .EnumerateFiles(buildRoot, "novel-gui-bridge.dll", SearchOption.AllDirectories)
                     .Select(path => new FileInfo(path))
                     .OrderByDescending(file => file.LastWriteTimeUtc)
                     .Select(file => file.FullName))
        {
            yield return path;
        }
    }

    private static IEnumerable<string> CandidateRoots()
    {
        yield return AppContext.BaseDirectory;
        yield return Directory.GetCurrentDirectory();

        var processPath = Environment.ProcessPath;
        if (!string.IsNullOrWhiteSpace(processPath))
        {
            var processDir = Path.GetDirectoryName(processPath);
            if (!string.IsNullOrWhiteSpace(processDir))
            {
                yield return processDir;
            }
        }
    }
}
