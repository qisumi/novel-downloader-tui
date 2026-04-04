const common = require("_shared/common");
const rainBatch = require("_shared/rain_batch");

const API_URL = "http://v3.rain.ink/fanqie/";
const WEB_BASE_URL = "https://v3.rain.ink";
const LOGIN_URL = `${WEB_BASE_URL}/web/index.php`;
const COVER_ORIGIN_URL = "https://p6-novel.byteimg.com/origin/";
const INVALID_KEY_MARKER = "无效的API密钥";
const USER_RE = /const user = (\{.*?\});/s;

const ctx = {
  api_key: "",
  logged_in: false,
  user_info: null,
};

const normalizeCoverUrl = (value) => String(value || "").trim();

const isDisplayableCoverUrl = (value) => {
  const url = normalizeCoverUrl(value);
  return !!url && !/\.hei[cf](?:$|[?#])/i.test(url);
};

const buildOriginCoverUrl = (thumbUri) => {
  const normalized = String(thumbUri || "").trim();
  if (!normalized) {
    return "";
  }
  if (/^https?:\/\//i.test(normalized)) {
    return normalized;
  }
  return `${COVER_ORIGIN_URL}${normalized.replace(/^\/+/, "")}`;
};

const pickCoverUrl = (data) => {
  const candidates = [
    common.getString(data, "thumb_url", ""),
    common.getString(data, "cover_url", ""),
    buildOriginCoverUrl(common.getString(data, "thumb_uri", "")),
  ];

  for (const candidate of candidates) {
    if (isDisplayableCoverUrl(candidate)) {
      return candidate;
    }
  }

  return "";
};

const parseBook = (data) => ({
  book_id: common.getString(data, "book_id", ""),
  title: common.getString(data, "book_name", common.getString(data, "title", "")),
  author: common.getString(data, "author", ""),
  cover_url: pickCoverUrl(data),
  abstract: common.getString(data, "abstract", ""),
  category: common.getString(data, "category", ""),
  word_count: common.getString(data, "word_number", common.getString(data, "word_count", "")),
  score: common.getNumber(data, "score", 0),
  gender: common.getNumber(data, "gender", 0),
  creation_status: common.getNumber(data, "creation_status", 0),
  last_chapter_title: common.getString(data, "last_chapter_title", ""),
  last_update_time: common.getNumber(data, "last_chapter_update_time", 0),
});

const ensureApiKey = async () => {
  if (!ctx.api_key) {
    ctx.api_key = await common.requireEnv("FANQIE_APIKEY");
  }
  return ctx.api_key;
};

const buildUrl = async (typeId, params) => {
  const query = {
    apikey: await ensureApiKey(),
    type: typeId,
    ...(params || {}),
  };
  return common.appendQuery(API_URL, query);
};

const parseUserInfo = (html) => {
  const match = String(html || "").match(USER_RE);
  if (!match) {
    return {};
  }

  try {
    return JSON.parse(match[1]);
  } catch (_error) {
    return {};
  }
};

const getAuthMarkers = (html) => {
  const text = String(html || "");
  return {
    invalidKey: text.includes(INVALID_KEY_MARKER),
    logout: text.includes("?logout=1"),
    user: text.includes("const user ="),
    keyword: text.includes('name="keyword"'),
    download: text.includes("downloadBtn"),
  };
};

const isAuthenticatedHtml = (markers) =>
  markers.user || (markers.logout && (markers.keyword || markers.download));

const assertAuthenticatedHtml = (html) => {
  const markers = getAuthMarkers(html);
  const text = String(html || "");
  if (text.includes(INVALID_KEY_MARKER)) {
    ctx.logged_in = false;
    host.config_error("番茄书源登录失败：API Key 无效");
  }

  if (!isAuthenticatedHtml(markers)) {
    throw new Error("__novel_data_error__:fanqie login did not return authenticated html");
  }
};

const requireLoggedIn = () => {
  if (!ctx.logged_in) {
    host.config_error("当前番茄书源尚未登录；请先登录后再启用批量下载");
  }
};

module.exports = {
  manifest: {
    id: "fanqie",
    name: "番茄小说",
    version: "1.3.2",
    author: "novel-downloader-tui",
    description: "默认番茄小说书源插件，会自行从环境变量或 .env 中读取 FANQIE_APIKEY",
    required_envs: ["FANQIE_APIKEY"],
    optional_envs: [],
  },

  async configure() {
    ctx.api_key = await common.requireEnv("FANQIE_APIKEY");
    ctx.logged_in = false;
    ctx.user_info = null;
  },

  async login() {
    const apiKey = await ensureApiKey();
    const response = await common.postForm(
      LOGIN_URL,
      { apikey: apiKey },
      {},
      30,
      false,
    );

    if (response.status >= 400) {
      throw new Error(`login request failed with status ${response.status}`);
    }
    if (String(response.body || "").includes(INVALID_KEY_MARKER)) {
      ctx.logged_in = false;
      host.config_error("番茄书源登录失败：API Key 无效");
    }

    await host.log_info(`fanqie login post status=${response.status}`);
    const verifyResponse = await host.http_request({
      method: "GET",
      url: LOGIN_URL,
      timeout_seconds: 30,
    });
    await host.log_info(`fanqie login verify status=${verifyResponse.status}`);
    const html = String(verifyResponse.body || "");
    const markers = getAuthMarkers(html);
    await host.log_info(
      `fanqie login markers logout=${markers.logout} user=${markers.user} keyword=${markers.keyword} download=${markers.download} invalid=${markers.invalidKey}`,
    );
    assertAuthenticatedHtml(html);

    // Rain Web 当前会先下发 server_name_session，再在验证页分配 PHPSESSID。
    // 对 batch.php 来说，仅有验证页生成的 PHPSESSID 还不够；需要再用现有 cookie
    // 补一次 apikey POST，才能把下载能力绑定到当前 PHP session。
    const bindResponse = await common.postForm(
      LOGIN_URL,
      { apikey: apiKey },
      {},
      30,
      false,
    );
    await host.log_info(`fanqie login bind status=${bindResponse.status}`);
    if (bindResponse.status >= 400) {
      throw new Error(`login bind request failed with status ${bindResponse.status}`);
    }

    ctx.user_info = parseUserInfo(html);
    ctx.logged_in = true;
    return true;
  },

  async search(keywords, page) {
    const root = await common.getJson(await buildUrl(1, {
      keywords,
      page: page * 10,
    }));

    const results = [];
    if (Array.isArray(root.search_tabs)) {
      for (const tab of root.search_tabs) {
        if (!Array.isArray(tab?.data)) {
          continue;
        }
        for (const item of tab.data) {
          const book = item?.book_data?.[0];
          if (book) {
            results.push(parseBook(book));
          }
        }
        if (results.length > 0) {
          break;
        }
      }
      return results;
    }

    if (Array.isArray(root.data)) {
      for (const item of root.data) {
        const book = item?.book_data?.[0] ?? item;
        results.push(parseBook(book));
      }
    }

    return results;
  },

  async get_book_info(bookId) {
    const root = await common.getJson(await buildUrl(2, {
      bookid: bookId,
    }));
    if (!root.data) {
      return null;
    }
    return parseBook(root.data);
  },

  async get_toc(bookId) {
    const root = await common.getJson(await buildUrl(3, {
      bookid: bookId,
    }));

    const items = root?.data?.item_data_list;
    if (!Array.isArray(items)) {
      return [];
    }

    return items.map((item) => ({
      item_id: common.getString(item, "item_id", ""),
      title: common.getString(item, "title", ""),
      volume_name: common.getString(item, "volume_name", ""),
      word_count: common.getNumber(item, "chapter_word_number", 0),
      update_time: common.getNumber(item, "first_pass_time", 0),
    }));
  },

  async get_chapter(_bookId, itemId) {
    const root = await common.getJson(await buildUrl(4, {
      itemid: itemId,
    }));
    if (!root?.data?.content) {
      return null;
    }
    return {
      item_id: itemId,
      title: "",
      content: String(root.data.content),
    };
  },

  async get_batch_count(bookId) {
    requireLoggedIn();
    return rainBatch.getBatchCount(WEB_BASE_URL, bookId);
  },

  async get_batch(bookId, batchNo) {
    requireLoggedIn();
    return rainBatch.getBatch(WEB_BASE_URL, bookId, batchNo);
  },
};
