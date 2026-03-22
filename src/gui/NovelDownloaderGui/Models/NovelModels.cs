using System.Text.Json;
using System.Text.Json.Serialization;

namespace NovelDownloaderGui.Models;

public sealed class BackendEnvelope<T>
{
    [JsonPropertyName("success")]
    public bool Success { get; set; }

    [JsonPropertyName("data")]
    public T? Data { get; set; }

    [JsonPropertyName("error")]
    public BackendError? Error { get; set; }
}

public sealed class BackendError
{
    [JsonPropertyName("kind")]
    public string Kind { get; set; } = string.Empty;

    [JsonPropertyName("message")]
    public string Message { get; set; } = string.Empty;
}

public sealed class BookRecord
{
    [JsonPropertyName("book_id")]
    public string BookId { get; set; } = string.Empty;

    [JsonPropertyName("title")]
    public string Title { get; set; } = string.Empty;

    [JsonPropertyName("author")]
    public string Author { get; set; } = string.Empty;

    [JsonPropertyName("cover_url")]
    public string CoverUrl { get; set; } = string.Empty;

    [JsonPropertyName("abstract")]
    public string Abstract { get; set; } = string.Empty;

    [JsonPropertyName("category")]
    public string Category { get; set; } = string.Empty;

    [JsonPropertyName("word_count")]
    public string WordCount { get; set; } = string.Empty;

    [JsonPropertyName("score")]
    public double Score { get; set; }

    [JsonPropertyName("gender")]
    public int Gender { get; set; }

    [JsonPropertyName("creation_status")]
    public int CreationStatus { get; set; }

    [JsonPropertyName("last_chapter_title")]
    public string LastChapterTitle { get; set; } = string.Empty;

    [JsonPropertyName("last_update_time")]
    public long LastUpdateTime { get; set; }
}

public sealed class TocItemRecord
{
    [JsonPropertyName("item_id")]
    public string ItemId { get; set; } = string.Empty;

    [JsonPropertyName("title")]
    public string Title { get; set; } = string.Empty;

    [JsonPropertyName("volume_name")]
    public string VolumeName { get; set; } = string.Empty;

    [JsonPropertyName("word_count")]
    public int WordCount { get; set; }

    [JsonPropertyName("update_time")]
    public long UpdateTime { get; set; }
}

public sealed class SourceInfoRecord
{
    [JsonPropertyName("id")]
    public string Id { get; set; } = string.Empty;

    [JsonPropertyName("name")]
    public string Name { get; set; } = string.Empty;

    [JsonPropertyName("version")]
    public string Version { get; set; } = string.Empty;

    [JsonPropertyName("author")]
    public string Author { get; set; } = string.Empty;

    [JsonPropertyName("description")]
    public string Description { get; set; } = string.Empty;

    [JsonPropertyName("required_envs")]
    public List<string> RequiredEnvs { get; set; } = [];

    [JsonPropertyName("optional_envs")]
    public List<string> OptionalEnvs { get; set; } = [];
}

public sealed class SourcesResponse
{
    [JsonPropertyName("current_source_id")]
    public string CurrentSourceId { get; set; } = string.Empty;

    [JsonPropertyName("sources")]
    public List<SourceInfoRecord> Sources { get; set; } = [];
}

public sealed class SearchResponse
{
    [JsonPropertyName("source_id")]
    public string SourceId { get; set; } = string.Empty;

    [JsonPropertyName("source_name")]
    public string SourceName { get; set; } = string.Empty;

    [JsonPropertyName("books")]
    public List<BookRecord> Books { get; set; } = [];
}

public sealed class BookshelfResponse
{
    [JsonPropertyName("source_id")]
    public string SourceId { get; set; } = string.Empty;

    [JsonPropertyName("books")]
    public List<BookRecord> Books { get; set; } = [];
}

public sealed class TocResponse
{
    [JsonPropertyName("book")]
    public BookRecord? Book { get; set; }

    [JsonPropertyName("toc")]
    public List<TocItemRecord> Toc { get; set; } = [];

    [JsonPropertyName("toc_count")]
    public int TocCount { get; set; }

    [JsonPropertyName("cached_chapter_count")]
    public int CachedChapterCount { get; set; }
}

public sealed class DownloadResponse
{
    [JsonPropertyName("downloaded_count")]
    public int DownloadedCount { get; set; }

    [JsonPropertyName("total_count")]
    public int TotalCount { get; set; }

    [JsonPropertyName("cached_chapter_count")]
    public int CachedChapterCount { get; set; }
}

public sealed class ExportResponse
{
    [JsonPropertyName("format")]
    public string Format { get; set; } = string.Empty;

    [JsonPropertyName("output_path")]
    public string OutputPath { get; set; } = string.Empty;

    [JsonPropertyName("toc_count")]
    public int TocCount { get; set; }
}
