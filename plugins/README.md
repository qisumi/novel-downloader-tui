# Plugins

默认插件目录。当前内置：

- `fanqie.lua`：番茄小说默认书源
- `qimao.lua`：七猫小说书源示例
- `_shared/common.lua`：共享 Lua helper，提供请求、JSON、字段兜底等通用能力

插件需要返回一个 Lua table，并至少包含：

- `manifest`
- `search(keywords, page)`
- `get_toc(book_id)`
- `get_chapter(book_id, item_id)`

可选：

- `configure(ctx)`
- `get_book_info(book_id)`

`manifest` 推荐包含：

- `id`
- `name`
- `version`
- `author`
- `description`
- `required_envs`
- `optional_envs`

其中：

- `required_envs`：插件运行必须提供的 `.env` / 环境变量名列表
- `optional_envs`：插件可选依赖的 `.env` / 环境变量名列表

宿主当前暴露给 Lua 的常用 API：

- `host.http_get(url)`
- `host.http_request({ method, url, headers?, body?, timeout_seconds? })`
- `host.json_parse(text)`
- `host.json_stringify(value)`
- `host.url_encode(text)`
- `host.env_get(name[, default])`
- `host.config_error(message)`
- `host.log_info(msg)`
- `host.log_warn(msg)`
- `host.log_error(msg)`

其中：

- `host.http_get(url)` 适合简单 GET，成功时返回响应 body，失败时返回 `nil, err`
- `host.http_request(...)` 适合复杂请求，成功时返回 `{ status, body, headers }`
- `body` 为 Lua table 时，宿主会自动编码为 JSON，并默认使用 `application/json`

示例：

```lua
local response, err = host.http_request({
    method = "POST",
    url = "https://example.com/api/search",
    headers = {
        Authorization = "Bearer " .. token,
    },
    body = {
        keyword = "番茄",
        page = 1,
    },
    timeout_seconds = 15,
})

if response == nil then
    error(err)
end

local data = host.json_parse(response.body)
```

推荐做法：

- `.env` 由宿主统一加载，插件通过 `host.env_get(...)` 读取配置
- 配置项是否存在、是否有效由插件自行校验
- 当配置缺失或非法时，调用 `host.config_error("...")` 生成面向用户的友好错误提示
- 书源特定的响应展开、字段清洗、结构兼容逻辑放在 Lua 插件内
- HTTP、JSON、日志、配置读取等通用能力优先由宿主提供
- 可复用逻辑优先放到 `plugins/_shared/*.lua`，插件内通过 `require("_shared.xxx")` 复用

共享 helper 示例：

```lua
local common = require("_shared.common")

local root = common.get_json("https://example.com/api/book?id=123")
local title = common.get_string(root.data, "title", "")
local score = common.get_number(root.data, "score", 0)
```

错误分类约定：

- `host.config_error(...)` 用于配置缺失、配置非法
- `host.http_get(...)` / `host.http_request(...)` 的网络失败会被宿主标记为网络错误
- `host.json_parse(...)` / `host.json_stringify(...)` 的失败会被宿主标记为数据处理错误
- 这些错误会在 C++ 侧继续补充书源 ID、操作名、插件路径，便于排查
