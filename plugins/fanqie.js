const common = require("_shared/common");

const API_URL = "http://v3.rain.ink/fanqie/";

const ctx = {
  api_key: "",
};

const parseBook = (data) => ({
  book_id: common.getString(data, "book_id", ""),
  title: common.getString(data, "book_name", common.getString(data, "title", "")),
  author: common.getString(data, "author", ""),
  cover_url: common.getString(data, "thumb_url", common.getString(data, "cover_url", "")),
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

module.exports = {
  manifest: {
    id: "fanqie",
    name: "番茄小说",
    version: "1.1.0",
    author: "novel-downloader-tui",
    description: "默认番茄小说书源插件，会自行从环境变量或 .env 中读取 FANQIE_APIKEY",
    required_envs: ["FANQIE_APIKEY"],
    optional_envs: [],
  },

  async configure() {
    ctx.api_key = await common.requireEnv("FANQIE_APIKEY");
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
};
