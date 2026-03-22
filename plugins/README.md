# Plugins

默认插件目录。当前内置：

- `fanqie.lua`：番茄小说默认书源

插件需要返回一个 Lua table，并至少包含：

- `manifest`
- `search(keywords, page)`
- `get_toc(book_id)`
- `get_chapter(book_id, item_id)`

可选：

- `configure(ctx)`
- `get_book_info(book_id)`

宿主当前暴露给 Lua 的常用 API：

- `host.http_get(url)`
- `host.json_parse(text)`
- `host.url_encode(text)`
- `host.env_get(name[, default])`
- `host.config_error(message)`
- `host.log_info(msg)`
- `host.log_warn(msg)`
- `host.log_error(msg)`

推荐做法：

- `.env` 由宿主统一加载，插件通过 `host.env_get(...)` 读取配置
- 配置项是否存在、是否有效由插件自行校验
- 当配置缺失或非法时，调用 `host.config_error("...")` 生成面向用户的友好错误提示
