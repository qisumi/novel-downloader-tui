local common = require("_shared.common")

local plugin = {}

plugin.manifest = {
    id = "qimao",
    name = "七猫小说",
    version = "1.0.0",
    author = "fanqie-downloader-tui",
    description = "七猫小说书源插件，会自行从环境变量或 .env 中读取 QIMAO_APIKEY",
    required_envs = {
        "QIMAO_APIKEY",
    },
    optional_envs = {},
}

local API_URL = "https://v3.rain.ink/qimao/"

local ctx = {
    api_key = "",
}

local function ensure_api_key()
    if ctx.api_key == "" then
        ctx.api_key = common.require_env("QIMAO_APIKEY")
    end
    return ctx.api_key
end

local function build_url(type_id, params)
    local query = {
        apikey = ensure_api_key(),
        type = type_id,
    }

    if params ~= nil then
        for key, value in pairs(params) do
            query[key] = value
        end
    end

    return common.append_query(API_URL, query)
end

local function parse_book(data)
    return {
        book_id = common.get_string(data, "id", common.get_string(data, "book_id", "")),
        title = common.get_string(data, "original_title", common.get_string(data, "title", "")),
        author = common.get_string(data, "original_author", common.get_string(data, "author", "")),
        cover_url = common.get_string(data, "image_link", common.get_string(data, "cover_url", "")),
        abstract = common.get_string(data, "intro", common.get_string(data, "abstract", "")),
        category = common.get_string(data, "category_over_words", common.get_string(data, "source", "")),
        word_count = common.get_string(data, "words_num", common.get_string(data, "word_count", "")),
        score = common.get_number(data, "score", 0),
        gender = common.get_number(data, "sex", 0),
        creation_status = common.get_number(data, "is_finish", common.get_number(data, "creation_status", 0)),
        last_chapter_title = common.get_string(data, "latest_chapter_title", ""),
        last_update_time = common.get_number(data, "update_time", 0),
    }
end

function plugin.configure()
    ctx.api_key = common.require_env("QIMAO_APIKEY")
end

function plugin.search(keywords, page)
    local root = common.get_json(build_url(1, {
        wd = keywords,
        page = page * 10,
    }))

    local results = {}
    local books = root.data and root.data.books or nil
    if books == nil then
        return results
    end

    for _, item in ipairs(books) do
        table.insert(results, parse_book(item))
    end

    return results
end

function plugin.get_book_info(book_id)
    local root = common.get_json(build_url(2, {
        id = book_id,
    }))
    local data = root.data and root.data.book or nil
    if data == nil then
        return nil
    end
    return parse_book(data)
end

function plugin.get_toc(book_id)
    local root = common.get_json(build_url(3, {
        id = book_id,
    }))

    local results = {}
    local chapters = root.data and root.data.chapter_lists or nil
    if chapters == nil then
        return results
    end

    for _, item in ipairs(chapters) do
        table.insert(results, {
            item_id = common.get_string(item, "id", ""),
            title = common.get_string(item, "title", ""),
            volume_name = common.get_string(item, "volume_name", ""),
            word_count = common.get_number(item, "words", 0),
            update_time = common.get_number(item, "update_time", 0),
        })
    end

    return results
end

function plugin.get_chapter(book_id, item_id)
    local root = common.get_json(build_url(4, {
        id = book_id,
        chapterid = item_id,
    }))
    if root.data == nil or root.data.content == nil then
        return nil
    end
    return {
        item_id = item_id,
        title = common.get_string(root.data, "title", ""),
        content = tostring(root.data.content),
    }
end

return plugin
