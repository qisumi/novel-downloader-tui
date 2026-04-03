#include "source/js/js_plugin_runtime.h"

#include <chrono>
#include <sstream>
#include <thread>
#include <utility>

#include <spdlog/spdlog.h>

#include "source/domain/source_errors.h"
#include "source/host/host_api.h"

namespace novel {

namespace {

using json = nlohmann::json;

/// 构造标准错误载荷，用于 JS 端接收错误信息
json make_error_payload(const std::string& message) {
    return {
        {"message", message},
    };
}

/// 从 JS 调用参数中提取指定位置的字符串参数，并进行类型校验
std::string require_string_arg(const json& args, std::size_t index, const char* name) {
    if (args.size() <= index || !args[index].is_string()) {
        throw std::runtime_error(prefix_source_error(
            SourceErrorCode::PluginRequestError,
            std::string(name) + " must be a string"));
    }
    return args[index].get<std::string>();
}

/// 从 JSON 对象构造 HttpRequest 结构体，校验字段类型
/// 支持解析 method、url、headers、body、timeout_seconds、follow_redirects 字段
HttpRequest request_from_json(const json& payload) {
    if (!payload.is_object()) {
        throw std::runtime_error(prefix_source_error(
            SourceErrorCode::PluginRequestError,
            "http_request expects an object"));
    }

    HttpRequest request;
    request.method = payload.value("method", "GET");
    if (request.method.empty()) {
        request.method = "GET";
    }

    if (!payload.contains("url") || !payload["url"].is_string()) {
        throw std::runtime_error(prefix_source_error(
            SourceErrorCode::PluginRequestError,
            "http_request requires a string url"));
    }
    request.url = payload["url"].get<std::string>();
    if (request.url.empty()) {
        throw std::runtime_error(prefix_source_error(
            SourceErrorCode::PluginRequestError,
            "http_request requires a non-empty url"));
    }

    // 解析可选的超时时间
    if (payload.contains("timeout_seconds") && !payload["timeout_seconds"].is_null()) {
        if (!payload["timeout_seconds"].is_number_integer()) {
            throw std::runtime_error(prefix_source_error(
                SourceErrorCode::PluginRequestError,
                "http_request timeout_seconds must be an integer"));
        }
        request.timeout_seconds = payload["timeout_seconds"].get<int>();
    }

    if (payload.contains("follow_redirects") && !payload["follow_redirects"].is_null()) {
        if (!payload["follow_redirects"].is_boolean()) {
            throw std::runtime_error(prefix_source_error(
                SourceErrorCode::PluginRequestError,
                "http_request follow_redirects must be a boolean"));
        }
        request.follow_redirects = payload["follow_redirects"].get<bool>();
    }

    // 解析可选的请求头，值支持字符串/整数/浮点/布尔类型
    if (payload.contains("headers") && !payload["headers"].is_null()) {
        if (!payload["headers"].is_object()) {
            throw std::runtime_error(prefix_source_error(
                SourceErrorCode::PluginRequestError,
                "http_request headers must be an object"));
        }
        for (const auto& [key, value] : payload["headers"].items()) {
            if (value.is_string()) {
                request.headers.emplace_back(key, value.get<std::string>());
                continue;
            }
            if (value.is_number_integer()) {
                request.headers.emplace_back(key, std::to_string(value.get<std::int64_t>()));
                continue;
            }
            if (value.is_number_unsigned()) {
                request.headers.emplace_back(key, std::to_string(value.get<std::uint64_t>()));
                continue;
            }
            if (value.is_number_float()) {
                request.headers.emplace_back(key, std::to_string(value.get<double>()));
                continue;
            }
            if (value.is_boolean()) {
                request.headers.emplace_back(key, value.get<bool>() ? "true" : "false");
                continue;
            }
            throw std::runtime_error(prefix_source_error(
                SourceErrorCode::PluginRequestError,
                "http_request header value must be a string/number/boolean"));
        }
    }

    // 解析可选的请求体：字符串直接使用，其他类型序列化为 JSON
    if (payload.contains("body") && !payload["body"].is_null()) {
        if (payload["body"].is_string()) {
            request.body = payload["body"].get<std::string>();
        } else {
            request.body = payload["body"].dump();
            request.content_type = "application/json";
        }
    }

    return request;
}

/// 将 HttpResponse 转换为 JSON 对象，供 JS 端使用
json response_to_json(const HttpResponse& response) {
    json headers = json::object();
    for (const auto& [key, value] : response.headers) {
        headers[key] = value;
    }

    return {
        {"status", response.status},
        {"body", response.body},
        {"headers", std::move(headers)},
    };
}

} // namespace

// ── 构造与安装 ─────────────

JsPluginRuntime::JsPluginRuntime(webview::webview& window, std::shared_ptr<HostApi> host_api)
    : window_(window), host_api_(std::move(host_api)) {}

/// 向 WebView 注入完整的 JS 插件运行时框架
/// 包括：host 对象（http_get/http_request/env_get/url_encode/log_*/config_error）、
/// 模块注册表、require() 加载器、invoke() RPC 调用器
void JsPluginRuntime::install() {
    window_.init(R"JS(
(() => {
  const runtimeKey = "__NOVEL_PLUGIN_HOST__";
  if (window[runtimeKey]) {
    window[runtimeKey].reset();
    return;
  }

  // 路径标准化：反斜杠转正斜杠
  const normalizePath = (value) => String(value || "").replace(/\\/g, "/");

  // RFC 3986 兼容的 URL 编码
  const encodeRfc3986 = (value) =>
    encodeURIComponent(String(value))
      .replace(/[!'()*]/g, (ch) => "%" + ch.charCodeAt(0).toString(16).toUpperCase());

  // 统一提取错误消息
  const errorMessage = (error) => {
    if (error && typeof error === "object" && typeof error.message === "string" && error.message.length > 0) {
      return error.message;
    }
    return String(error);
  };

  // 模块 ID 标准化：去除 .js 后缀、解析 . 和 .. 路径段
  const normalizeModuleId = (value) => {
    let moduleId = normalizePath(value);
    if (moduleId.endsWith(".js")) {
      moduleId = moduleId.slice(0, -3);
    }

    const stack = [];
    for (const segment of moduleId.split("/")) {
      if (!segment || segment === ".") {
        continue;
      }
      if (segment === "..") {
        if (stack.length > 0) {
          stack.pop();
        }
        continue;
      }
      stack.push(segment);
    }
    return stack.join("/");
  };

  // ── host 对象：暴露给 JS 插件的宿主 API ─────────────
  const host = {
    async http_get(url, headers = {}, timeoutSeconds = 30) {
      const response = await this.http_request({
        method: "GET",
        url,
        headers,
        timeout_seconds: timeoutSeconds,
      });
      if (response.status < 200 || response.status >= 300) {
        throw new Error(`request failed with status ${response.status}: ${String(url)}`);
      }
      return response.body;
    },
    async http_request(request) {
      try {
        return await window.native_plugin_http_request(request);
      } catch (error) {
        throw new Error(errorMessage(error));
      }
    },
    async env_get(name, fallback = null) {
      try {
        return await window.native_plugin_env_get(name, fallback);
      } catch (error) {
        throw new Error(errorMessage(error));
      }
    },
    url_encode(value) {
      return encodeRfc3986(value);
    },
    async log_info(message) {
      await window.native_plugin_log("info", String(message));
    },
    async log_warn(message) {
      await window.native_plugin_log("warn", String(message));
    },
    async log_error(message) {
      await window.native_plugin_log("error", String(message));
    },
    config_error(message) {
      throw new Error("__novel_config_error__:" + String(message));
    },
  };

  // ── 插件运行时核心对象 ─────────────
  window[runtimeKey] = {
    modules: new Map(),   // 模块注册表（module_id → {pluginPath, source}）
    cache: new Map(),     // 模块缓存（module_id → module.exports）
    host,
    reset() {
      this.modules.clear();
      this.cache.clear();
    },
    // 注册单个模块
    registerModule(moduleId, pluginPath, source) {
      const normalized = normalizeModuleId(moduleId);
      this.modules.set(normalized, {
        pluginPath: normalizePath(pluginPath),
        source: String(source),
      });
      this.cache.delete(normalized);
    },
    // 解析模块路径（支持相对路径 ./ ../）
    resolve(specifier, fromModuleId = "") {
      const normalizedSpecifier = normalizeModuleId(specifier);
      const baseParts = normalizeModuleId(fromModuleId).split("/").filter(Boolean);
      if (baseParts.length > 0) {
        baseParts.pop();
      }

      const isRelative =
        String(specifier).startsWith("./") || String(specifier).startsWith("../");

      const parts = isRelative ? baseParts.slice() : [];
      for (const segment of normalizedSpecifier.split("/").filter(Boolean)) {
        parts.push(segment);
      }

      const candidate = parts.join("/");
      if (this.modules.has(candidate)) {
        return candidate;
      }
      if (this.modules.has(candidate + "/index")) {
        return candidate + "/index";
      }

      throw new Error(`cannot resolve module "${String(specifier)}" from "${String(fromModuleId)}"`);
    },
    // CommonJS 风格的 require 实现
    require(specifier, fromModuleId = "") {
      const moduleId = this.resolve(specifier, fromModuleId);
      if (this.cache.has(moduleId)) {
        return this.cache.get(moduleId).exports;
      }

      const record = this.modules.get(moduleId);
      if (!record) {
        throw new Error(`module not found: ${moduleId}`);
      }

      const module = { exports: {} };
      this.cache.set(moduleId, module);

      const localRequire = (childSpecifier) => this.require(childSpecifier, moduleId);
      // 使用 Function 构造器执行模块源码，注入 module/exports/require/host
      const runner = new Function(
        "module",
        "exports",
        "require",
        "host",
        `${record.source}\n//# sourceURL=${record.pluginPath}`
      );

      runner(module, module.exports, localRequire, this.host);
      return module.exports;
    },
    // RPC 调用入口：加载插件 → 调用方法 → 返回结果/错误
    async invoke(payload) {
      const callId = String(payload.call_id);
      try {
        const plugin = this.require(payload.module_id);
        const operation = String(payload.operation);
        if (!plugin || typeof plugin !== "object") {
          throw new Error("plugin module must export an object");
        }

        const fn = plugin[operation];
        if (typeof fn !== "function") {
          throw new Error(`missing plugin method: ${operation}`);
        }

        const args = Array.isArray(payload.args) ? payload.args : [];
        const result = await Promise.resolve(fn.apply(plugin, args));
        await window.native_plugin_result({
          call_id: callId,
          ok: true,
          result: result === undefined ? null : result,
        });
      } catch (error) {
        await window.native_plugin_result({
          call_id: callId,
          ok: false,
          message: errorMessage(error),
        });
      }
    },
  };
})();
)JS");

    // ── 绑定 C++ 端回调函数，供 JS 端调用 ─────────────

    // 处理 JS 端的 HTTP 请求调用
    bind_async("native_plugin_http_request", [this](const json& args) {
        const auto request = request_from_json(args.at(0));
        const auto response = host_api_->http_request(request);
        if (!response) {
            throw std::runtime_error(prefix_source_error(
                SourceErrorCode::NetworkError,
                "request failed: " + request.method + " " + request.url));
        }
        return response_to_json(*response);
    });

    // 处理 JS 端的环境变量读取
    bind_async("native_plugin_env_get", [this](const json& args) {
        const auto name = require_string_arg(args, 0, "name");
        std::optional<std::string> fallback = std::nullopt;
        if (args.size() > 1 && !args[1].is_null()) {
            if (!args[1].is_string()) {
                throw std::runtime_error(prefix_source_error(
                    SourceErrorCode::PluginRequestError,
                    "fallback must be a string or null"));
            }
            fallback = args[1].get<std::string>();
        }

        const auto value = host_api_->env_get(name, fallback);
        return value ? json(*value) : json(nullptr);
    });

    // 处理 JS 端的日志输出
    bind_async("native_plugin_log", [this](const json& args) {
        const auto level = require_string_arg(args, 0, "level");
        const auto message = require_string_arg(args, 1, "message");

        if (level == "info") {
            host_api_->log_info(message);
        } else if (level == "warn") {
            host_api_->log_warn(message);
        } else {
            host_api_->log_error(message);
        }

        return json(nullptr);
    });

    // 处理 JS 端的 RPC 调用结果回调
    window_.bind("native_plugin_result", [this](std::string raw_args) {
        const auto args = parse_args(raw_args);
        if (args.empty() || !args[0].is_object()) {
            return json(nullptr).dump();
        }

        const auto& payload = args[0];
        const auto call_id = payload.value("call_id", "");
        if (call_id.empty()) {
            return json(nullptr).dump();
        }

        {
            std::lock_guard lock(calls_mutex_);
            auto it = pending_calls_.find(call_id);
            if (it != pending_calls_.end()) {
                it->second.completed = true;
                if (payload.value("ok", false)) {
                    it->second.result = payload.value("result", json(nullptr));
                } else {
                    it->second.error = payload.value("message", "plugin call failed");
                }
            }
        }

        calls_cv_.notify_all();
        return json(nullptr).dump();
    });

    // 处理 JS 端的引导完成回调，收集所有成功加载的插件信息
    window_.bind("native_plugin_bootstrap", [this](std::string raw_args) {
        const auto args = parse_args(raw_args);
        if (args.empty() || !args[0].is_object()) {
            return json(nullptr).dump();
        }

        const auto& payload = args[0];
        {
            std::lock_guard lock(bootstrap_mutex_);
            bootstrap_finished_ = true;
            bootstrap_failed_ = !payload.value("ok", false);
            bootstrap_plugins_.clear();
            bootstrap_error_.clear();

            if (bootstrap_failed_) {
                bootstrap_error_ = payload.value("message", "plugin bootstrap failed");
            } else {
                // 记录加载失败的插件错误日志
                if (payload.contains("errors") && payload["errors"].is_array()) {
                    for (const auto& item : payload["errors"]) {
                        spdlog::error("Failed to evaluate JS plugin {}: {}",
                                      item.value("plugin_path", ""),
                                      item.value("message", "unknown error"));
                    }
                }

                // 收集成功加载的插件及其 manifest 和方法信息
                if (payload.contains("plugins") && payload["plugins"].is_array()) {
                    for (const auto& item : payload["plugins"]) {
                        JsBootstrapPlugin plugin;
                        plugin.module_id = item.value("module_id", "");
                        plugin.plugin_path = item.value("plugin_path", "");
                        plugin.manifest = item.value("manifest", json(nullptr));
                        plugin.has_configure = item.value("has_configure", false);
                        plugin.has_login = item.value("has_login", false);
                        plugin.has_search = item.value("has_search", false);
                        plugin.has_book_info = item.value("has_book_info", false);
                        plugin.has_toc = item.value("has_toc", false);
                        plugin.has_chapter = item.value("has_chapter", false);
                        plugin.has_batch_count = item.value("has_batch_count", false);
                        plugin.has_batch = item.value("has_batch", false);
                        bootstrap_plugins_.push_back(std::move(plugin));
                    }
                }
            }
        }

        bootstrap_cv_.notify_all();
        return json(nullptr).dump();
    });
}

// ── 模块注册与引导 ─────────────

/// 将 JS 模块逐个注册到 WebView 的模块表中
void JsPluginRuntime::queue_modules(const std::vector<JsModule>& modules) {
    for (const auto& module : modules) {
        window_.init(
            "window.__NOVEL_PLUGIN_HOST__.registerModule("
            + json(module.module_id).dump() + ", "
            + json(module.plugin_path).dump() + ", "
            + json(module.source).dump() + ");");
    }
}

/// 触发引导流程：注入 JS 代码依次 require 候选插件、检测导出方法、收集 manifest
void JsPluginRuntime::queue_bootstrap(const std::vector<JsModule>& plugins) {
    {
        std::lock_guard lock(bootstrap_mutex_);
        bootstrap_finished_ = false;
        bootstrap_failed_ = false;
        bootstrap_error_.clear();
        bootstrap_plugins_.clear();
    }

    // 构建候选插件列表的 JSON 数组
    json candidates = json::array();
    for (const auto& plugin : plugins) {
        candidates.push_back({
            {"module_id", plugin.module_id},
            {"plugin_path", plugin.plugin_path},
        });
    }

    // 注入引导脚本：逐个加载插件并检测导出方法，最终通过 native_plugin_bootstrap 回传结果
    window_.init(
        "(async () => {"
        "  try {"
        "    const candidates = " + candidates.dump() + ";"
        "    const plugins = [];"
        "    const errors = [];"
        "    for (const candidate of candidates) {"
        "      try {"
        "        const plugin = window.__NOVEL_PLUGIN_HOST__.require(candidate.module_id);"
        "        plugins.push({"
        "          module_id: candidate.module_id,"
        "          plugin_path: candidate.plugin_path,"
        "          manifest: plugin && typeof plugin === 'object' ? (plugin.manifest ?? null) : null,"
        "          has_configure: !!(plugin && typeof plugin.configure === 'function'),"
        "          has_login: !!(plugin && typeof plugin.login === 'function'),"
        "          has_search: !!(plugin && typeof plugin.search === 'function'),"
        "          has_book_info: !!(plugin && typeof plugin.get_book_info === 'function'),"
        "          has_toc: !!(plugin && typeof plugin.get_toc === 'function'),"
        "          has_chapter: !!(plugin && typeof plugin.get_chapter === 'function'),"
        "          has_batch_count: !!(plugin && typeof plugin.get_batch_count === 'function'),"
        "          has_batch: !!(plugin && typeof plugin.get_batch === 'function')"
        "        });"
        "      } catch (error) {"
        "        errors.push({"
        "          module_id: candidate.module_id,"
        "          plugin_path: candidate.plugin_path,"
        "          message: error && error.message ? error.message : String(error)"
        "        });"
        "      }"
        "    }"
        "    await window.native_plugin_bootstrap({ ok: true, plugins, errors });"
        "  } catch (error) {"
        "    await window.native_plugin_bootstrap({"
        "      ok: false,"
        "      message: error && error.message ? error.message : String(error)"
        "    });"
        "  }"
        "})();");
}

// ── 同步等待与 RPC 调用 ─────────────

/// 阻塞等待引导流程完成（超时 30 秒）
/// 返回成功加载的插件列表；超时或失败时抛出异常
std::vector<JsBootstrapPlugin> JsPluginRuntime::wait_for_bootstrap() {
    std::unique_lock lock(bootstrap_mutex_);
    if (!bootstrap_cv_.wait_for(lock, std::chrono::seconds(30), [&] {
            return bootstrap_finished_;
        })) {
        throw std::runtime_error("JS plugin bootstrap timed out");
    }

    if (bootstrap_failed_) {
        throw std::runtime_error(bootstrap_error_);
    }

    return bootstrap_plugins_;
}

/// 调用指定插件的某个方法（异步 RPC）
/// 1. 生成唯一 call_id，创建 PendingCall 条目
/// 2. 通过 WebView dispatch 注入调用脚本
/// 3. 阻塞等待 JS 端通过 native_plugin_result 回传结果（超时 60 秒）
JsPluginRuntime::json JsPluginRuntime::call(
    const std::string& module_id,
    const std::string& operation,
    const json& args) {
    const std::string call_id = [&] {
        std::lock_guard lock(calls_mutex_);
        std::ostringstream oss;
        oss << "plugin-call-" << next_call_id_++;
        pending_calls_.emplace(oss.str(), PendingCall{});
        return oss.str();
    }();

    const std::string script =
        "window.__NOVEL_PLUGIN_HOST__.invoke(" +
        json({
            {"call_id", call_id},
            {"module_id", module_id},
            {"operation", operation},
            {"args", args},
        }).dump() +
        ");";

    // 在 WebView 主线程中执行调用脚本
    window_.dispatch([this, script] {
        window_.eval(script);
    });

    // 阻塞等待调用完成
    std::unique_lock lock(calls_mutex_);
    if (!calls_cv_.wait_for(lock, std::chrono::seconds(60), [&] {
            auto it = pending_calls_.find(call_id);
            return it != pending_calls_.end() && it->second.completed;
        })) {
        pending_calls_.erase(call_id);
        throw std::runtime_error("JS plugin call timed out");
    }

    auto it = pending_calls_.find(call_id);
    if (it == pending_calls_.end()) {
        throw std::runtime_error("JS plugin call lost");
    }

    PendingCall pending = std::move(it->second);
    pending_calls_.erase(it);

    if (pending.error) {
        throw std::runtime_error(*pending.error);
    }
    return pending.result.value_or(json(nullptr));
}

// ── 内部工具方法 ─────────────

/// 以异步线程方式绑定 WebView JS 回调
/// 将耗时的 C++ 处理（如 HTTP 请求）放到独立线程，避免阻塞 WebView 主线程
void JsPluginRuntime::bind_async(
    const std::string& name,
    std::function<json(const json&)> handler) {
    window_.bind(name, [this, name, handler = std::move(handler)](
                           std::string call_id,
                           std::string raw_args,
                           void*) {
        std::thread([this,
                     call_id = std::move(call_id),
                     raw_args = std::move(raw_args),
                     handler,
                     name]() mutable {
            try {
                const auto payload = handler(parse_args(raw_args));
                window_.resolve(call_id, 0, payload.dump());
            } catch (const std::exception& e) {
                spdlog::warn("JS runtime binding failed [{}]: {}", name, e.what());
                window_.resolve(call_id, 1, make_error_payload(e.what()).dump());
            }
        }).detach();
    }, nullptr);
}

/// 解析 WebView 传来的原始 JSON 字符串为 JSON 数组
JsPluginRuntime::json JsPluginRuntime::parse_args(const std::string& raw_args) {
    auto parsed = json::parse(raw_args.empty() ? "[]" : raw_args);
    if (!parsed.is_array()) {
        throw std::runtime_error(prefix_source_error(
            SourceErrorCode::PluginRequestError,
            "arguments must be an array"));
    }
    return parsed;
}

} // namespace novel
