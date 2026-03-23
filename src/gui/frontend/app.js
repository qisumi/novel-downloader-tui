const state = {
  selectedBook: null,
  toc: [],
};

const sourceSelect = document.querySelector("#source-select");
const refreshSourcesButton = document.querySelector("#refresh-sources");
const searchForm = document.querySelector("#search-form");
const searchInput = document.querySelector("#search-input");
const statusLine = document.querySelector("#status-line");
const searchResults = document.querySelector("#search-results");
const detailView = document.querySelector("#book-detail");
const tocSummary = document.querySelector("#toc-summary");
const tocList = document.querySelector("#toc-list");
const bookshelfList = document.querySelector("#bookshelf-list");
const taskFeed = document.querySelector("#task-feed");
const exportStart = document.querySelector("#export-start");
const exportEnd = document.querySelector("#export-end");
const exportFormat = document.querySelector("#export-format");
const exportsDir = document.querySelector("#exports-dir");

function setStatus(text) {
  statusLine.textContent = text;
}

function showError(error) {
  const payload = error?.error || error;
  const message = payload?.message || String(error);
  const code = payload?.code ? ` (${payload.code})` : "";
  const sourceId = payload?.details?.source_id ? ` [${payload.details.source_id}]` : "";
  setStatus(`失败${code}${sourceId}: ${message}`);
}

async function callApp(method, ...args) {
  const raw = await window.app[method](...args);
  const result = typeof raw === "string" ? JSON.parse(raw) : raw;

  if (result?.ok === false) {
    throw result;
  }

  if (result && typeof result === "object" && "data" in result) {
    return result.data;
  }
  return result;
}

function renderSources(payload) {
  sourceSelect.innerHTML = "";
  const sources = Array.isArray(payload?.sources) ? payload.sources : [];
  const currentSourceId = payload?.current_source_id || "";

  if (!sources.length) {
    const option = document.createElement("option");
    option.value = "";
    option.textContent = "未发现可用书源";
    option.disabled = true;
    option.selected = true;
    sourceSelect.append(option);
    sourceSelect.disabled = true;
    return;
  }

  sourceSelect.disabled = false;
  for (const source of sources) {
    const option = document.createElement("option");
    option.value = source.id;
    option.textContent = `${source.name} (${source.id})`;
    option.selected = source.id === currentSourceId;
    sourceSelect.append(option);
  }
}

function renderBooks(items) {
  searchResults.innerHTML = "";
  if (!items.length) {
    searchResults.innerHTML = `<div class="empty-state">没有找到结果</div>`;
    return;
  }

  for (const book of items) {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "book-card-button";
    button.innerHTML = `
      <strong>${book.title || "未命名书籍"}</strong>
      <div class="meta">${book.author || "未知作者"}</div>
      <div>${book.abstract || "暂无简介"}</div>
      <div class="meta">${book.category || "未分类"} · ${book.word_count || "字数未知"}</div>
    `;
    button.addEventListener("click", () => selectBook(book));
    searchResults.append(button);
  }
}

function renderBookDetail(book) {
  if (!book) {
    detailView.className = "book-detail empty-state";
    detailView.textContent = "选择一本书查看详情";
    return;
  }

  detailView.className = "book-detail";
  detailView.innerHTML = `
    <div class="title">${book.title || "未命名书籍"}</div>
    <div class="meta">${book.author || "未知作者"}</div>
    <div>${book.abstract || "暂无简介"}</div>
    <div class="chips">
      <span class="chip">分类: ${book.category || "未分类"}</span>
      <span class="chip">字数: ${book.word_count || "-"}</span>
      <span class="chip">评分: ${book.score ?? 0}</span>
      <span class="chip">最新章节: ${book.last_chapter_title || "-"}</span>
    </div>
  `;
}

function renderToc(payload) {
  state.toc = payload.items;
  tocSummary.textContent = `共 ${payload.toc_count} 章，已缓存 ${payload.cached_chapter_count} 章`;
  exportStart.max = String(Math.max(1, payload.items.length));
  exportEnd.max = String(Math.max(1, payload.items.length));
  exportEnd.value = String(Math.max(1, payload.items.length));

  tocList.innerHTML = "";
  if (!payload.items.length) {
    tocList.innerHTML = `<div class="empty-state">暂无目录</div>`;
    return;
  }

  payload.items.forEach((item, index) => {
    const row = document.createElement("div");
    row.className = `toc-item ${item.cached ? "cached" : ""}`;
    row.innerHTML = `
      <strong>${String(index + 1).padStart(3, "0")} · ${item.title || "未命名章节"}</strong>
      <div class="meta">${item.volume_name || "默认卷"} · ${item.word_count || 0} 字${item.cached ? " · 已缓存" : ""}</div>
    `;
    tocList.append(row);
  });
}

function renderBookshelf(items) {
  bookshelfList.innerHTML = "";
  if (!items.length) {
    bookshelfList.innerHTML = `<div class="empty-state">书架还是空的</div>`;
    return;
  }

  for (const book of items) {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "bookshelf-item";
    button.innerHTML = `
      <strong>${book.title || "未命名书籍"}</strong>
      <div class="meta">${book.author || "未知作者"}</div>
    `;
    button.addEventListener("click", () => selectBook(book));
    bookshelfList.append(button);
  }
}

function pushTask(detail) {
  const item = document.createElement("div");
  item.className = "task-item";
  const progress = detail.current && detail.total ? `${detail.current}/${detail.total}` : "";
  item.innerHTML = `
    <strong>${detail.kind} · ${detail.stage}</strong>
    <div>${detail.book_id || detail.path || ""}</div>
    <div class="meta">${progress}</div>
  `;
  taskFeed.prepend(item);
  while (taskFeed.children.length > 8) {
    taskFeed.removeChild(taskFeed.lastChild);
  }
}

async function loadSources() {
  const payload = await callApp("get_sources");
  renderSources(payload);
  const count = Array.isArray(payload?.sources) ? payload.sources.length : 0;
  setStatus(`已加载 ${count} 个书源`);
}

async function loadBookshelf() {
  const payload = await callApp("list_bookshelf");
  renderBookshelf(payload.items);
}

async function selectBook(book) {
  state.selectedBook = book;
  renderBookDetail(book);
  setStatus(`正在加载《${book.title || book.book_id}》`);

  try {
    const detail = await callApp("get_book_detail", book.book_id);
    state.selectedBook = detail.book;
    renderBookDetail(detail.book);

    const toc = await callApp("get_toc", detail.book.book_id, false);
    renderToc(toc);
    setStatus(`已载入《${detail.book.title || detail.book.book_id}》`);
  } catch (error) {
    showError(error);
  }
}

searchForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  const keywords = searchInput.value.trim();
  if (!keywords) {
    setStatus("请输入关键词");
    return;
  }

  setStatus(`正在搜索: ${keywords}`);
  try {
    const payload = await callApp("search_books", keywords, 0);
    renderBooks(payload.items);
    setStatus(`搜索完成，共 ${payload.items.length} 本`);
  } catch (error) {
    showError(error);
  }
});

refreshSourcesButton.addEventListener("click", async () => {
  try {
    await loadSources();
    setStatus("书源已刷新");
  } catch (error) {
    showError(error);
  }
});

sourceSelect.addEventListener("change", async () => {
  try {
    const payload = await callApp("select_source", sourceSelect.value);
    setStatus(`已切换到书源: ${payload.source.name}`);
    await loadBookshelf();
  } catch (error) {
    showError(error);
  }
});

document.querySelector("#reload-toc").addEventListener("click", async () => {
  if (!state.selectedBook) {
    setStatus("请先选择书籍");
    return;
  }
  try {
    const payload = await callApp("get_toc", state.selectedBook.book_id, true);
    renderToc(payload);
    setStatus("目录已刷新");
  } catch (error) {
    showError(error);
  }
});

document.querySelector("#download-book").addEventListener("click", async () => {
  if (!state.selectedBook) {
    setStatus("请先选择书籍");
    return;
  }
  try {
    const payload = await callApp("download_book", state.selectedBook);
    setStatus(`下载完成，已缓存 ${payload.cached_chapter_count} 章`);
    const toc = await callApp("get_toc", state.selectedBook.book_id, false);
    renderToc(toc);
  } catch (error) {
    showError(error);
  }
});

document.querySelector("#export-book").addEventListener("click", async () => {
  if (!state.selectedBook) {
    setStatus("请先选择书籍");
    return;
  }

  const start = Math.max(1, Number(exportStart.value || 1));
  const end = Math.max(start, Number(exportEnd.value || start));

  try {
    const payload = await callApp(
      "export_book",
      state.selectedBook,
      start - 1,
      end - 1,
      exportFormat.value,
    );
    exportsDir.textContent = payload.exports_dir;
    setStatus(`导出完成: ${payload.path}`);
  } catch (error) {
    showError(error);
  }
});

document.querySelector("#save-bookshelf").addEventListener("click", async () => {
  if (!state.selectedBook) {
    setStatus("请先选择书籍");
    return;
  }
  try {
    await callApp("save_bookshelf", state.selectedBook);
    await loadBookshelf();
    setStatus("已加入书架");
  } catch (error) {
    showError(error);
  }
});

document.querySelector("#remove-bookshelf").addEventListener("click", async () => {
  if (!state.selectedBook) {
    setStatus("请先选择书籍");
    return;
  }
  try {
    await callApp("remove_bookshelf", state.selectedBook.book_id);
    await loadBookshelf();
    setStatus("已移出书架");
  } catch (error) {
    showError(error);
  }
});

window.addEventListener("novel:task", (event) => {
  pushTask(event.detail);
});

async function bootstrap() {
  try {
    await loadSources();
    await loadBookshelf();
    exportsDir.textContent = "导出后显示";
    setStatus("GUI 已就绪");
  } catch (error) {
    showError(error);
  }
}

bootstrap();
