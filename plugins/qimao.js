const common = require("_shared/common");

const API_URL = "https://v3.rain.ink/qimao/";

const ctx = {
  api_key: "",
};

const ensureApiKey = async () => {
  if (!ctx.api_key) {
    ctx.api_key = await common.requireEnv("QIMAO_APIKEY");
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

const parseBook = (data) => ({
  book_id: common.getString(data, "id", common.getString(data, "book_id", "")),
  title: common.getString(data, "original_title", common.getString(data, "title", "")),
  author: common.getString(data, "original_author", common.getString(data, "author", "")),
  cover_url: common.getString(data, "image_link", common.getString(data, "cover_url", "")),
  abstract: common.getString(data, "intro", common.getString(data, "abstract", "")),
  category: common.getString(data, "category_over_words", common.getString(data, "source", "")),
  word_count: common.getString(data, "words_num", common.getString(data, "word_count", "")),
  score: common.getNumber(data, "score", 0),
  gender: common.getNumber(data, "sex", 0),
  creation_status: common.getNumber(data, "is_finish", common.getNumber(data, "creation_status", 0)),
  last_chapter_title: common.getString(data, "latest_chapter_title", ""),
  last_update_time: common.getNumber(data, "update_time", 0),
});

module.exports = {
  manifest: {
    id: "qimao",
    name: "七猫小说",
    version: "1.0.0",
    author: "novel-downloader-tui",
    description: "七猫小说书源插件，会自行从环境变量或 .env 中读取 QIMAO_APIKEY",
    required_envs: ["QIMAO_APIKEY"],
    optional_envs: [],
  },

  async configure() {
    ctx.api_key = await common.requireEnv("QIMAO_APIKEY");
  },

  async search(keywords, page) {
    const root = await common.getJson(await buildUrl(1, {
      wd: keywords,
      page: page * 10,
    }));

    const books = root?.data?.books;
    if (!Array.isArray(books)) {
      return [];
    }

    return books.map((item) => parseBook(item));
  },

  async get_book_info(bookId) {
    const root = await common.getJson(await buildUrl(2, {
      id: bookId,
    }));
    const data = root?.data?.book;
    if (!data) {
      return null;
    }
    return parseBook(data);
  },

  async get_toc(bookId) {
    const root = await common.getJson(await buildUrl(3, {
      id: bookId,
    }));

    const chapters = root?.data?.chapter_lists;
    if (!Array.isArray(chapters)) {
      return [];
    }

    return chapters.map((item) => ({
      item_id: common.getString(item, "id", ""),
      title: common.getString(item, "title", ""),
      volume_name: common.getString(item, "volume_name", ""),
      word_count: common.getNumber(item, "words", 0),
      update_time: common.getNumber(item, "update_time", 0),
    }));
  },

  async get_chapter(bookId, itemId) {
    const root = await common.getJson(await buildUrl(4, {
      id: bookId,
      chapterid: itemId,
    }));
    if (!root?.data?.content) {
      return null;
    }
    return {
      item_id: itemId,
      title: common.getString(root.data, "title", ""),
      content: String(root.data.content),
    };
  },
};
