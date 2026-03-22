using System.Runtime.InteropServices;
using System.Reflection;
using System.Text;
using System.Text.Json;
using NovelDownloaderGui.Helpers;
using NovelDownloaderGui.Models;

namespace NovelDownloaderGui.Services;

public sealed class BackendService
{
    private static readonly JsonSerializerOptions JsonOptions = new(JsonSerializerDefaults.Web)
    {
        PropertyNameCaseInsensitive = true,
    };
    private const string BridgeLibraryName = "novel-gui-bridge";
    private static readonly string BridgeLibraryPath;

    static BackendService()
    {
        BridgeLibraryPath = PathLocator.FindBridgeLibrary();
        NativeLibrary.SetDllImportResolver(typeof(BackendService).Assembly, ResolveLibrary);
    }

    public Task<SourcesResponse> GetSourcesAsync(AppSettings settings) =>
        ExecuteAsync<SourcesResponse>(settings, new
        {
            command = "sources",
        });

    public Task<SearchResponse> SearchBooksAsync(AppSettings settings, string keywords, int page = 0) =>
        ExecuteAsync<SearchResponse>(settings, new
        {
            command = "search",
            keywords,
            page,
        });

    public Task<BookshelfResponse> GetBookshelfAsync(AppSettings settings) =>
        ExecuteAsync<BookshelfResponse>(settings, new
        {
            command = "bookshelf.list",
        });

    public Task SaveBookAsync(AppSettings settings, BookRecord book) =>
        ExecuteAsync<JsonElement>(
            settings,
            new
            {
                command = "bookshelf.save",
                book_json = JsonSerializer.Serialize(book, JsonOptions),
            });

    public Task RemoveBookAsync(AppSettings settings, string bookId) =>
        ExecuteAsync<JsonElement>(settings, new
        {
            command = "bookshelf.remove",
            book_id = bookId,
        });

    public Task<TocResponse> LoadTocAsync(AppSettings settings, BookRecord book, bool forceRemote) =>
        ExecuteAsync<TocResponse>(
            settings,
            new
            {
                command = "toc",
                book_json = JsonSerializer.Serialize(book, JsonOptions),
                force_remote = forceRemote,
            });

    public Task<DownloadResponse> DownloadBookAsync(AppSettings settings, BookRecord book, bool forceRemote) =>
        ExecuteAsync<DownloadResponse>(
            settings,
            new
            {
                command = "download",
                book_json = JsonSerializer.Serialize(book, JsonOptions),
                force_remote = forceRemote,
            });

    public Task<ExportResponse> ExportBookAsync(
        AppSettings settings,
        BookRecord book,
        int start,
        int end,
        string format,
        bool forceRemote) =>
        ExecuteAsync<ExportResponse>(
            settings,
            new
            {
                command = "export",
                book_json = JsonSerializer.Serialize(book, JsonOptions),
                start,
                end,
                format,
                force_remote = forceRemote,
            });

    private static async Task<T> ExecuteAsync<T>(AppSettings settings, object payload)
    {
        Directory.CreateDirectory(settings.OutputDirectory);

        var requestJson = BuildRequestJson(settings, payload);
        var responseJson = await Task.Run(() => InvokeBridge(requestJson));

        var envelope = JsonSerializer.Deserialize<BackendEnvelope<T>>(responseJson, JsonOptions);
        if (envelope is null)
        {
            throw new InvalidOperationException("无法解析 bridge 返回的数据。");
        }

        if (!envelope.Success)
        {
            throw new InvalidOperationException(envelope.Error?.Message ?? "bridge 执行失败。");
        }

        if (envelope.Data is null)
        {
            throw new InvalidOperationException("bridge 返回了空结果。");
        }

        return envelope.Data;
    }

    private static string BuildRequestJson(AppSettings settings, object payload)
    {
        var request = new Dictionary<string, object?>
        {
            ["db_path"] = settings.DatabasePath,
            ["plugin_dir"] = settings.PluginDirectory,
            ["output_dir"] = settings.OutputDirectory,
            ["source_id"] = settings.SourceId,
        };

        foreach (var property in payload.GetType().GetProperties())
        {
            request[property.Name] = property.GetValue(payload);
        }

        return JsonSerializer.Serialize(request, JsonOptions);
    }

    private static string InvokeBridge(string requestJson)
    {
        if (string.IsNullOrWhiteSpace(BridgeLibraryPath) || !File.Exists(BridgeLibraryPath))
        {
            throw new InvalidOperationException(
                "未找到 novel-gui-bridge.dll，请先构建 bridge，或使用打包目录中的 GUI。");
        }

        var responsePtr = novel_gui_invoke(requestJson);
        if (responsePtr == IntPtr.Zero)
        {
            throw new InvalidOperationException("bridge 没有返回数据。");
        }

        try
        {
            return Marshal.PtrToStringUTF8(responsePtr) ?? string.Empty;
        }
        finally
        {
            novel_gui_free(responsePtr);
        }
    }

    private static IntPtr ResolveLibrary(string libraryName, Assembly assembly, DllImportSearchPath? searchPath)
    {
        if (!string.Equals(libraryName, BridgeLibraryName, StringComparison.OrdinalIgnoreCase))
        {
            return IntPtr.Zero;
        }

        if (string.IsNullOrWhiteSpace(BridgeLibraryPath) || !File.Exists(BridgeLibraryPath))
        {
            return IntPtr.Zero;
        }

        return NativeLibrary.Load(BridgeLibraryPath);
    }

    [DllImport(BridgeLibraryName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "novel_gui_invoke")]
    private static extern IntPtr novel_gui_invoke([MarshalAs(UnmanagedType.LPUTF8Str)] string requestJson);

    [DllImport(BridgeLibraryName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "novel_gui_free")]
    private static extern void novel_gui_free(IntPtr text);
}
