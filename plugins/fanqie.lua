local common = require("_shared.common")

local plugin = {}

plugin.manifest = {
    id = "fanqie",
    name = "番茄小说",
    version = "1.1.0",
    author = "fanqie-downloader-tui",
    description = "默认番茄小说书源插件，会自行从环境变量或 .env 中读取 FANQIE_APIKEY",
    required_envs = {
        "FANQIE_APIKEY",
    },
    optional_envs = {},
}

local API_URL = "http://v3.rain.ink/fanqie/"

local ctx = {
    api_key = "",
}

local function parse_book(data)
    return {
        book_id = common.get_string(data, "book_id", ""),
        title = common.get_string(data, "book_name", common.get_string(data, "title", "")),
        author = common.get_string(data, "author", ""),
        cover_url = common.get_string(data, "thumb_url", common.get_string(data, "cover_url", "")),
        abstract = common.get_string(data, "abstract", ""),
        category = common.get_string(data, "category", ""),
        word_count = common.get_string(data, "word_number", common.get_string(data, "word_count", "")),
        score = common.get_number(data, "score", 0),
        gender = common.get_number(data, "gender", 0),
        creation_status = common.get_number(data, "creation_status", 0),
        last_chapter_title = common.get_string(data, "last_chapter_title", ""),
        last_update_time = common.get_number(data, "last_chapter_update_time", 0),
    }
end

local function ensure_api_key()
    if ctx.api_key == "" then
        ctx.api_key = common.require_env("FANQIE_APIKEY")
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

function plugin.configure()
    ctx.api_key = common.require_env("FANQIE_APIKEY")
end

function plugin.search(keywords, page)
    local root = common.get_json(build_url(1, {
        keywords = keywords,
        page = page * 10,
    }))

    local results = {}

    if root.search_tabs ~= nil then
        for _, tab in ipairs(root.search_tabs) do
            if tab.data ~= nil then
                for _, item in ipairs(tab.data) do
                    if item.book_data ~= nil and item.book_data[1] ~= nil then
                        table.insert(results, parse_book(item.book_data[1]))
                    end
                end
                if #results > 0 then
                    break
                end
            end
        end
    elseif root.data ~= nil then
        for _, item in ipairs(root.data) do
            if item.book_data ~= nil and item.book_data[1] ~= nil then
                table.insert(results, parse_book(item.book_data[1]))
            else
                table.insert(results, parse_book(item))
            end
        end
    end

    return results
end

function plugin.get_book_info(book_id)
    local root = common.get_json(build_url(2, {
        bookid = book_id,
    }))
    if root.data == nil then
        return nil
    end
    return parse_book(root.data)
end

function plugin.get_toc(book_id)
    local root = common.get_json(build_url(3, {
        bookid = book_id,
    }))

    local results = {}
    local items = root.data and root.data.item_data_list or nil
    if items == nil then
        return results
    end

    for _, item in ipairs(items) do
        table.insert(results, {
            item_id = common.get_string(item, "item_id", ""),
            title = common.get_string(item, "title", ""),
            volume_name = common.get_string(item, "volume_name", ""),
            word_count = common.get_number(item, "chapter_word_number", 0),
            update_time = common.get_number(item, "first_pass_time", 0),
        })
    end

    return results
end

function plugin.get_chapter(book_id, item_id)
    local root = common.get_json(build_url(4, {
        itemid = item_id,
    }))
    if root.data == nil or root.data.content == nil then
        return nil
    end
    return {
        item_id = item_id,
        title = "",
        content = tostring(root.data.content),
    }
end

return plugin
