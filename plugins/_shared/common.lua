local common = {}

local function scalar_to_string(value)
    if value == nil then
        return ""
    end
    return tostring(value)
end

function common.safe_get(tbl, key, fallback)
    if type(tbl) ~= "table" then
        return fallback
    end

    local value = tbl[key]
    if value == nil then
        return fallback
    end
    return value
end

function common.get_string(tbl, key, fallback)
    local value = common.safe_get(tbl, key, nil)
    if value == nil then
        return fallback or ""
    end
    return tostring(value)
end

function common.get_number(tbl, key, fallback)
    local value = common.safe_get(tbl, key, nil)
    local number = tonumber(value)
    if number == nil then
        return fallback or 0
    end
    return number
end

function common.require_env(name)
    local value = host.env_get(name, "") or ""
    if value == "" then
        host.config_error("missing " .. name .. "; please set it in the environment or .env")
    end
    return value
end

function common.append_query(base_url, params)
    if params == nil then
        return base_url
    end

    local parts = {}
    for key, value in pairs(params) do
        if value ~= nil and value ~= "" then
            table.insert(parts, host.url_encode(tostring(key)) .. "=" .. host.url_encode(scalar_to_string(value)))
        end
    end

    if #parts == 0 then
        return base_url
    end

    table.sort(parts)
    local separator = string.find(base_url, "?", 1, true) and "&" or "?"
    return base_url .. separator .. table.concat(parts, "&")
end

function common.request_json(request)
    local response, err = host.http_request(request)
    if response == nil then
        error(err or ("request failed: " .. scalar_to_string(request.url)))
    end
    if response.status < 200 or response.status >= 300 then
        error("request failed with status " .. tostring(response.status) .. ": " .. scalar_to_string(request.url))
    end

    return host.json_parse(response.body), response
end

function common.get_json(url, headers, timeout_seconds)
    return common.request_json({
        method = "GET",
        url = url,
        headers = headers,
        timeout_seconds = timeout_seconds,
    })
end

return common
