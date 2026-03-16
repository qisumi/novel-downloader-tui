local plugin = {}

plugin.manifest = {
    id = "fanqie",
    name = "番茄小说",
    version = "1.0.0",
    author = "fanqie-downloader-tui",
    description = "默认番茄小说书源插件，需要提供API Key，编写到.env文件中，或者在插件配置中填写",
    requires_api_key = true,
}

local ctx = {
    api_key = "",
}

local function safe_get(tbl, key, fallback)
    local value = tbl[key]
    if value == nil then
        return fallback
    end
    return value
end

local function parse_book(data)
    return {
        book_id = tostring(safe_get(data, "book_id", "")),
        title = tostring(safe_get(data, "book_name", safe_get(data, "title", ""))),
        author = tostring(safe_get(data, "author", "")),
        cover_url = tostring(safe_get(data, "thumb_url", safe_get(data, "cover_url", ""))),
        abstract = tostring(safe_get(data, "abstract", "")),
        category = tostring(safe_get(data, "category", "")),
        word_count = tostring(safe_get(data, "word_number", safe_get(data, "word_count", ""))),
        score = tonumber(safe_get(data, "score", 0)) or 0,
        gender = tonumber(safe_get(data, "gender", 0)) or 0,
        creation_status = tonumber(safe_get(data, "creation_status", 0)) or 0,
        last_chapter_title = tostring(safe_get(data, "last_chapter_title", "")),
        last_update_time = tonumber(safe_get(data, "last_chapter_update_time", 0)) or 0,
    }
end

local function request(url)
    local body, err = host.http_get(url)
    if body == nil then
        error(err or ("request failed: " .. url))
    end
    return host.json_parse(body)
end

local function build_url(type_id, extra_params)
    local url = "http://v3.rain.ink/fanqie/?apikey=" .. ctx.api_key .. "&type=" .. tostring(type_id)
    if extra_params ~= nil and extra_params ~= "" then
        url = url .. "&" .. extra_params
    end
    return url
end

function plugin.configure(new_ctx)
    ctx.api_key = new_ctx.api_key or ""
    if ctx.api_key == "" then
        ctx.api_key = host.env_get("FANQIE_APIKEY", "") or ""
    end
end

function plugin.search(keywords, page)
    local encoded = host.url_encode(keywords)
    local root = request(build_url(1, "keywords=" .. encoded .. "&page=" .. tostring(page * 10)))
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
    local root = request(build_url(2, "bookid=" .. host.url_encode(book_id)))
    if root.data == nil then
        return nil
    end
    return parse_book(root.data)
end

function plugin.get_toc(book_id)
    local root = request(build_url(3, "bookid=" .. host.url_encode(book_id)))
    local results = {}
    local items = root.data and root.data.item_data_list or nil
    if items == nil then
        return results
    end

    for _, item in ipairs(items) do
        table.insert(results, {
            item_id = tostring(safe_get(item, "item_id", "")),
            title = tostring(safe_get(item, "title", "")),
            volume_name = tostring(safe_get(item, "volume_name", "")),
            word_count = tonumber(safe_get(item, "chapter_word_number", 0)) or 0,
            update_time = tonumber(safe_get(item, "first_pass_time", 0)) or 0,
        })
    end
    return results
end

function plugin.get_chapter(book_id, item_id)
    local root = request(build_url(4, "itemid=" .. host.url_encode(item_id)))
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
