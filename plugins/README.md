# Plugins

默认插件目录。当前内置：

- `fanqie.js`：番茄小说默认书源
- `qimao.js`：七猫小说书源示例
- `_shared/common.js`：共享 JS helper，提供请求、字段兜底、URL 拼接等通用能力

## 插件结构

插件文件是普通 CommonJS 风格 JS 模块，需通过 `module.exports` 导出一个对象，并至少包含：

- `manifest`
- `search(keywords, page)`
- `get_toc(book_id)`
- `get_chapter(book_id, item_id)`

可选：

- `configure()`：无参数，由宿主在当前书源首次就绪后调用，用于校验配置、初始化状态等
- `get_book_info(book_id)`

插件方法可以返回普通值，也可以返回 `Promise`。

### 基本模板

```js
const common = require("_shared/common");

module.exports = {
  manifest: {
    id: "fanqie",
    name: "番茄小说",
    version: "1.1.0",
    required_envs: ["FANQIE_APIKEY"],
    optional_envs: [],
  },

  async configure() {},
  async search(keywords, page) {},
  async get_book_info(bookId) {},
  async get_toc(bookId) {},
  async get_chapter(bookId, itemId) {},
};
```

### manifest

`manifest` 必须是一个 object，其中：

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| `id` | string | 是 | 书源唯一标识，不可为空，重复 ID 的插件会被跳过 |
| `name` | string | 是 | 书源显示名称，不可为空 |
| `version` | string | 否 | 版本号 |
| `author` | string | 否 | 作者 |
| `description` | string | 否 | 描述 |
| `required_envs` | string[] | 否 | 插件运行必须提供的 `.env` / 环境变量名列表 |
| `optional_envs` | string[] | 否 | 插件可选依赖的 `.env` / 环境变量名列表 |

如果 `manifest` 不是 object，或 `id` / `name` 缺失、为空，插件会被拒绝并报 `PluginInvalidManifest` 错误。

## 返回值结构

### search(keywords, page) → Book[]

返回一个对象数组。每个 `Book` 字段：

| 字段 | 类型 | 必需 | 默认值 | 说明 |
|------|------|------|--------|------|
| `book_id` | string | 是 | — | 书籍唯一标识 |
| `title` | string | 是 | — | 书名 |
| `author` | string | 否 | `""` | 作者 |
| `cover_url` | string | 否 | `""` | 封面 URL |
| `abstract` | string | 否 | `""` | 简介 |
| `category` | string | 否 | `""` | 分类 |
| `word_count` | string | 否 | `""` | 字数 |
| `score` | number | 否 | `0.0` | 评分 |
| `gender` | integer | 否 | `0` | 0=未知, 1=男频, 2=女频 |
| `creation_status` | integer | 否 | `0` | 0=连载, 1=完结 |
| `last_chapter_title` | string | 否 | `""` | 最新章节标题 |
| `last_update_time` | integer | 否 | `0` | Unix 时间戳 |

缺少必需字段会抛出 `InvalidReturnField` 错误。

### get_book_info(book_id) → Book | null

返回结构与 `search` 相同的单个对象，或 `null`。此函数为可选，若插件未定义则宿主返回空。

### get_toc(book_id) → TocItem[]

返回目录项数组。每个 `TocItem` 的字段：

| 字段 | 类型 | 必需 | 默认值 | 说明 |
|------|------|------|--------|------|
| `item_id` | string | 是 | — | 章节 ID |
| `title` | string | 是 | — | 章节标题 |
| `volume_name` | string | 否 | `""` | 卷名 |
| `word_count` | integer | 否 | `0` | 字数 |
| `update_time` | integer | 否 | `0` | Unix 时间戳 |

### get_chapter(book_id, item_id) → Chapter | null

返回章节对象，或 `null`。字段：

| 字段 | 类型 | 必需 | 默认值 | 说明 |
|------|------|------|--------|------|
| `content` | string | 是 | — | 正文内容 |
| `item_id` | string | 否 | `""` | 章节 ID，为空时自动填充调用参数 |
| `title` | string | 否 | `""` | 章节标题 |

## 宿主 API

宿主向插件暴露 `host.*`：

| API | 说明 |
|-----|------|
| `host.http_get(url, headers?, timeoutSeconds?)` | 简单 GET 请求；非 2xx 时抛异常 |
| `host.http_request(opts)` | 完整 HTTP 请求，支持 GET/POST/PUT/PATCH/DELETE/HEAD |
| `host.url_encode(text)` | URL 编码（RFC 3986，保留 `-_.~`） |
| `host.env_get(name, defaultValue?)` | 读取环境变量，未找到时返回 `defaultValue` 或 `null` |
| `host.config_error(message)` | 报告配置错误，直接抛异常 |
| `host.log_info(msg)` | 记录 info 级日志 |
| `host.log_warn(msg)` | 记录 warn 级日志 |
| `host.log_error(msg)` | 记录 error 级日志 |

### host.http_request(opts) 参数

`opts` 为一个 object：

| 字段 | 类型 | 必需 | 默认值 | 说明 |
|------|------|------|--------|------|
| `url` | string | 是 | — | 请求 URL |
| `method` | string | 否 | `"GET"` | HTTP 方法 |
| `headers` | object | 否 | `{}` | 请求头，string key-value |
| `body` | string/object | 否 | `null` | string 直接发送；object 自动编码为 JSON 并设 `Content-Type: application/json` |
| `timeout_seconds` | integer | 否 | `30` | 超时秒数 |

成功时返回 `{ status, body, headers }`。
网络失败时直接抛异常。

### 示例

```js
const response = await host.http_request({
  method: "POST",
  url: "https://example.com/api/search",
  headers: {
    Authorization: `Bearer ${token}`,
  },
  body: {
    keyword: "番茄",
    page: 1,
  },
  timeout_seconds: 15,
});

const data = JSON.parse(response.body);
```

## 共享模块

以下划线开头的路径段（如 `_shared/`）不会作为书源插件加载，但可被其他插件 `require(...)` 引用：

```js
const common = require("_shared/common");

const root = await common.getJson("https://example.com/api/book?id=123");
const title = common.getString(root.data, "title", "");
const score = common.getNumber(root.data, "score", 0);
```

插件运行时会自动注册 `plugins/` 下的所有 `.js` 模块，因此 `require("_shared/common")`、`require("./helper")` 这类写法可以直接工作。

## 推荐做法

- `.env` 由宿主统一加载，插件通过 `host.env_get(...)` 读取配置
- 配置项是否存在、是否有效由插件自行校验
- 当配置缺失或非法时，调用 `host.config_error("...")` 生成面向用户的友好错误提示
- 书源特定的响应展开、字段清洗、结构兼容逻辑放在 JS 插件内
- HTTP、日志、配置读取等通用能力优先由宿主提供
- 可复用逻辑优先放到 `plugins/_shared/*.js`，插件内通过 `require(...)` 复用

## 错误分类

宿主对 JS 侧错误有如下分类：

| 错误类型 | 触发场景 |
|----------|----------|
| `PluginInvalidManifest` | manifest 不是 object、缺少 id/name |
| `PluginMissingMethod` | 缺少 search / get_toc / get_chapter |
| `PluginConfigError` | 调用 `host.config_error(...)` |
| `PluginRequestError` | `host.http_request(...)` 参数非法 |
| `PluginDataError` | JSON 解析或插件数据处理失败 |
| `NetworkError` | HTTP 请求网络失败 |
| `PluginRuntimeError` | 其他未分类的 JS 运行时错误 |
| `InvalidReturnField` | 返回对象缺少必需字段或字段类型错误 |

这些错误会在 C++ 侧自动补充书源 ID、操作名、插件路径，便于排查定位。
