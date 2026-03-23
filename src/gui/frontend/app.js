const state = {
  selectedBook: null,
  toc: [],
  bookshelfItems: [],
  currentPage: 'search-page',
  isDetailVisible: false
};

// Nav & Pages
const navItems = document.querySelectorAll('.nav-item');
const pageViews = document.querySelectorAll('.page-view');
const detailPage = document.getElementById('detail-page');
const backButton = document.getElementById('back-button');

// Selectors
const sourceSelect = document.querySelector('#source-select');
const refreshSourcesButton = document.querySelector('#refresh-sources');
const searchForm = document.querySelector('#search-form');
const searchInput = document.querySelector('#search-input');
const statusLine = document.querySelector('#status-line');
const searchResults = document.querySelector('#search-results');
const detailView = document.querySelector('#book-detail');
const tocSummary = document.querySelector('#toc-summary');
const tocList = document.querySelector('#toc-list');
const bookshelfList = document.querySelector('#bookshelf-list');
const taskFeed = document.querySelector('#task-feed');
const exportStart = document.querySelector('#export-start');
const exportEnd = document.querySelector('#export-end');
const exportFormat = document.querySelector('#export-format');
const exportsDir = document.querySelector('#exports-dir');
const saveBookshelfBtn = document.querySelector('#save-bookshelf');
const removeBookshelfBtn = document.querySelector('#remove-bookshelf');

function switchPage(targetId) {
  state.currentPage = targetId;
  if (state.isDetailVisible) {
    hideDetailPage();
  }

  // Update nav UI
  navItems.forEach(item => {
    if (item.dataset.target === targetId) {
      item.classList.add("active");
    } else {
      item.classList.remove("active");
    }
  });

  syncPageVisibility();
}

function syncPageVisibility() {
  pageViews.forEach(page => {
    if (page.id === 'detail-page') return;

    page.classList.remove('detail-active');
    page.style.display = '';

    if (page.id === state.currentPage) {
      page.classList.add('active');
    } else {
      page.classList.remove('active');
    }
  });
}

function showDetailPage() {
  state.isDetailVisible = true;

  pageViews.forEach(page => {
    if (page.id !== 'detail-page') {
      page.classList.add('detail-active');
      page.classList.remove('active');
      page.style.display = '';
    }
  });

  detailPage.classList.add('active');
  detailPage.style.display = '';
}

function hideDetailPage() {
  state.isDetailVisible = false;
  detailPage.classList.remove('active');
  detailPage.style.display = '';

  syncPageVisibility();
}

// Nav Listeners
navItems.forEach(item => {
  item.addEventListener('click', () => {
    switchPage(item.dataset.target);
  });
});

if (backButton) {
  backButton.addEventListener('click', () => {
    hideDetailPage();
  });
}

function setStatus(text, isError = false) {
  statusLine.textContent = text;
  statusLine.style.color = isError ? '#d32f2f' : 'var(--brand-deep)';
}

function showError(error) {
  const payload = error?.error || error;
  const message = payload?.message || String(error);
  const code = payload?.code ? ` (${payload.code})` : '';
  const sourceId = payload?.details?.source_id ? ` [${payload.details.source_id}]` : '';
  setStatus(`失败${code}${sourceId}: ${message}`, true);
}

async function callApp(method, ...args) {
  const raw = await window.app[method](...args);
  const result = typeof raw === 'string' ? JSON.parse(raw) : raw;

  if (result?.ok === false) {
    throw result;
  }

  if (result && typeof result === 'object' && 'data' in result) {
    return result.data;
  }
  return result;
}

function renderSources(payload) {
  sourceSelect.innerHTML = '';
  const sources = Array.isArray(payload?.sources) ? payload.sources : [];
  const currentSourceId = payload?.current_source_id || '';

  if (!sources.length) {
    const option = document.createElement('option');
    option.value = '';
    option.textContent = '未发现可用书源';
    option.disabled = true;
    option.selected = true;
    sourceSelect.append(option);
    sourceSelect.disabled = true;
    return;
  }

  sourceSelect.disabled = false;
  for (const source of sources) {
    const option = document.createElement('option');
    option.value = source.id;
    option.textContent = source.name;
    option.selected = source.id === currentSourceId;
    sourceSelect.append(option);
  }
}

function renderBooks(items, container, emptyMessage) {
  if (!container) return;
  container.innerHTML = '';
  if (!items.length) {
    container.innerHTML = `<div class="empty-state" style="grid-column: 1 / -1;">${emptyMessage}</div>`;
    return;
  }

  for (const book of items) {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'book-card-button';
    
    let abstractHtml = '';
    if (book.abstract) {
        abstractHtml = `<div style="margin-top: 4px; font-size: 0.9rem; color: var(--text); display: -webkit-box; -webkit-line-clamp: 2; -webkit-box-orient: vertical; overflow: hidden;">${book.abstract}</div>`;
    }

    button.innerHTML = `
      <strong>${book.title || "未命名书籍"}</strong>
      <div class="meta">${book.author || "未知作者"}</div>
      ${abstractHtml}
      <div class="meta" style="margin-top: auto; padding-top: 8px;">${book.category || "未分类"} · ${book.word_count || "未知"} 字</div>
    `;
    button.addEventListener('click', () => selectBook(book));
    container.append(button);
  }
}

function updateBookshelfButtons(bookId) {
    if (!saveBookshelfBtn || !removeBookshelfBtn) return;
    const inBookshelf = state.bookshelfItems.some(b => b.book_id === bookId);
    if (inBookshelf) {
        saveBookshelfBtn.style.display = 'none';
        removeBookshelfBtn.style.display = 'block';
    } else {
        saveBookshelfBtn.style.display = 'block';
        removeBookshelfBtn.style.display = 'none';
    }
}

function renderBookDetail(book) {
  if (!book || !detailView) return;

  detailView.className = 'book-detail';
  detailView.innerHTML = `
    <div class="title">${book.title || "未命名书籍"}</div>
    <div class="meta" style="font-size: 1rem;">${book.author || "未知作者"}</div>
    <div class="chips" style="margin: 8px 0;">
      <span class="chip">分类: ${book.category || "未分类"}</span>
      <span class="chip">字数: ${book.word_count || "-"}</span>
      <span class="chip">评分: ${book.score ?? 0}</span>
    </div>
    <div class="meta" style="margin-bottom: 8px;">最新章节: ${book.last_chapter_title || "-"}</div>
    <div style="line-height: 1.6; margin-top: auto; color: var(--text); background: rgba(255,255,255,0.4); padding: 12px; border-radius: 8px; border: 1px solid var(--line); font-size: 0.95rem; display: -webkit-box; -webkit-line-clamp: 8; -webkit-box-orient: vertical; overflow: hidden; text-overflow: ellipsis;" title="简介">${book.abstract || "暂无简介"}</div>
  `;
}

function renderToc(payload) {
  state.toc = payload.items;
  if(tocSummary) tocSummary.textContent = `共 ${payload.toc_count || payload.items.length} 章，已缓存 ${payload.cached_chapter_count || 0} 章`;
  if(exportStart) exportStart.max = String(Math.max(1, payload.items.length));
  if(exportEnd) {
      exportEnd.max = String(Math.max(1, payload.items.length));
      exportEnd.value = String(Math.max(1, payload.items.length));
  }

  if(!tocList) return;
  tocList.innerHTML = '';
  if (!payload.items.length) {
    tocList.innerHTML = `<div class="empty-state">暂无目录数据</div>`;
    return;
  }

  payload.items.forEach((item, index) => {
    const row = document.createElement('div');
    row.className = `toc-item ${item.cached ? "cached" : ""}`;
    row.innerHTML = `
      <strong>${String(index + 1).padStart(3, "0")} · ${item.title || "未命名章节"}</strong>
      <div class="meta">${item.volume_name || "默认卷"} · ${item.word_count || 0} 字${item.cached ? " · 已缓存" : ""}</div>
    `;
    tocList.append(row);
  });
}

function pushTask(detail) {
  if(!taskFeed) return;
  const emptyState = taskFeed.querySelector('.empty-state');
  if (emptyState) {
      taskFeed.removeChild(emptyState);
  }

  const item = document.createElement('div');
  item.className = 'task-item';
  const progress = detail.current && detail.total ? `${detail.current}/${detail.total}` : '';
  item.innerHTML = `
    <strong>${detail.kind} · ${detail.stage}</strong>
    <div style="font-family: monospace; font-size: 0.8rem; word-break: break-all; margin-top: 4px;">${detail.book_id || detail.path || ""}</div>
    <div class="meta" style="margin-top: 4px; font-weight: 500;">${progress}</div>
  `;
  taskFeed.prepend(item);
  while (taskFeed.children.length > 20) {
    taskFeed.removeChild(taskFeed.lastChild);
  }
}

async function loadSources() {
  const payload = await callApp('get_sources');
  renderSources(payload);
  const count = Array.isArray(payload?.sources) ? payload.sources.length : 0;
  setStatus(`已加载 ${count} 个书源`);
}

async function loadBookshelf() {
  const payload = await callApp('list_bookshelf');
  state.bookshelfItems = payload.items || [];
  renderBooks(state.bookshelfItems, bookshelfList, '书架空空如也，去探索一下吧~');
  if (state.selectedBook) {
      updateBookshelfButtons(state.selectedBook.book_id);
  }
}

async function selectBook(book) {
  state.selectedBook = book;
  showDetailPage();
  renderBookDetail(book);
  updateBookshelfButtons(book.book_id);

  if(tocSummary) tocSummary.textContent = '加载目录中...';
  if(tocList) tocList.innerHTML = `<div class="empty-state">正在同步书籍信息...</div>`;
  setStatus(`正在加载《${book.title || book.book_id}》...`);

  try {
    const detail = await callApp('get_book_detail', book.book_id);
    state.selectedBook = detail.book;
    renderBookDetail(detail.book);
    updateBookshelfButtons(detail.book.book_id);

    const toc = await callApp('get_toc', detail.book.book_id, false);
    renderToc(toc);
    setStatus(`已载入《${detail.book.title || detail.book.book_id}》`);
  } catch (error) {
    showError(error);
    if(tocList) tocList.innerHTML = `<div class="empty-state" style="color:#d32f2f;">加载目录失败</div>`;
  }
}

if(searchForm) {
    searchForm.addEventListener('submit', async (event) => {
    event.preventDefault();
    const keywords = searchInput.value.trim();
    if (!keywords) {
        setStatus('请输入关键词', true);
        return;
    }

    setStatus(`正在搜索: ${keywords}...`);
    const submitBtn = searchForm.querySelector('button');
    submitBtn.disabled = true;
    submitBtn.textContent = '搜索中';
    searchResults.innerHTML = `<div class="empty-state" style="grid-column: 1 / -1;">正在检索，请稍候...</div>`;
    try {
        const payload = await callApp('search_books', keywords, 0);
        renderBooks(payload.items, searchResults, '没有找到相关结果，换个词试试？');
        setStatus(`搜索完成，共找到 ${payload.items.length} 本书籍`);
    } catch (error) {
        showError(error);
        searchResults.innerHTML = `<div class="empty-state" style="grid-column: 1 / -1; color:#d32f2f;">搜索失败</div>`;
    } finally {
        submitBtn.disabled = false;
        submitBtn.textContent = '立即搜索';
    }
    });
}

if(refreshSourcesButton) {
    refreshSourcesButton.addEventListener('click', async () => {
    const btn = refreshSourcesButton;
    btn.disabled = true;
    try {
        await loadSources();
        setStatus('书源已刷新');
    } catch (error) {
        showError(error);
    } finally {
        btn.disabled = false;
    }
    });
}

if(sourceSelect) {
    sourceSelect.addEventListener('change', async () => {
    try {
        const payload = await callApp('select_source', sourceSelect.value);
        setStatus(`已切换书源: ${payload.source?.name || sourceSelect.value}`);
        await loadBookshelf();
    } catch (error) {
        showError(error);
    }
    });
}

const reloadTocBtn = document.querySelector('#reload-toc');
if(reloadTocBtn) {
    reloadTocBtn.addEventListener('click', async () => {
    if (!state.selectedBook) return;
    const btn = reloadTocBtn;
    btn.disabled = true;
    try {
        setStatus('正在获取最新目录...');
        const payload = await callApp('get_toc', state.selectedBook.book_id, true);
        renderToc(payload);
        setStatus('目录已更新到最新');
    } catch (error) {
        showError(error);
    } finally {
        btn.disabled = false;
    }
    });
}

const downloadBtn = document.querySelector('#download-book');
if(downloadBtn) {
    downloadBtn.addEventListener('click', async () => {
    if (!state.selectedBook) return;
    const btn = downloadBtn;
    btn.disabled = true;
    try {
        setStatus('开始下载任务...');
        switchPage('tasks-page');
        const payload = await callApp('download_book', state.selectedBook);
        setStatus('下载请求已完成');

        const toc = await callApp('get_toc', state.selectedBook.book_id, false);
        renderToc(toc);
    } catch (error) {
        showError(error);
    } finally {
        btn.disabled = false;
    }
    });
}

const exportBtn = document.querySelector('#export-book');
if(exportBtn) {
    exportBtn.addEventListener('click', async () => {
    if (!state.selectedBook) return;

    const start = Math.max(1, Number(exportStart.value || 1));
    const end = Math.max(start, Number(exportEnd.value || start));

    const btn = exportBtn;
    btn.disabled = true;
    try {
        setStatus('正在导出，请稍候...');
        switchPage('tasks-page');

        const payload = await callApp(
        'export_book',
        state.selectedBook,
        start - 1,
        end - 1,
        exportFormat.value,
        );
        if(exportsDir) exportsDir.textContent = payload.exports_dir;
        setStatus(`导出成功，保存在: ${payload.path || payload.exports_dir}`);
    } catch (error) {
        showError(error);
    } finally {
        btn.disabled = false;
    }
    });
}

if(saveBookshelfBtn) {
    saveBookshelfBtn.addEventListener('click', async () => {
    if (!state.selectedBook) return;
    try {
        await callApp('save_bookshelf', state.selectedBook);
        await loadBookshelf();
        setStatus('已成功加入书架');
    } catch (error) {
        showError(error);
    }
    });
}

if(removeBookshelfBtn) {
    removeBookshelfBtn.addEventListener('click', async () => {
    if (!state.selectedBook) return;
    try {
        await callApp('remove_bookshelf', state.selectedBook.book_id);
        await loadBookshelf();
        setStatus('已从书架移除');
    } catch (error) {
        showError(error);
    }
    });
}

window.addEventListener('novel:task', (event) => {
  pushTask(event.detail);
});

async function bootstrap() {
  try {
    await loadSources();
    await loadBookshelf();

    // Initialize navigation state
    switchPage('search-page');

    if(exportsDir) exportsDir.textContent = '导出后将显示目录';
    setStatus('系统就绪');
  } catch (error) {
    showError(error);
  }
}

bootstrap();

