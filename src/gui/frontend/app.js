const state = {
  selectedBook: null,
  toc: [],
  bookshelfItems: [],
  tasks: [],
  currentPage: 'search-page',
  isDetailVisible: false,
  sourceCapabilities: null
};

// Nav & Pages
const navItems = document.querySelectorAll('.nav-item');
const pageViews = document.querySelectorAll('.page-view');
const detailPage = document.getElementById('detail-page');
const backButton = document.getElementById('back-button');

// Selectors
const sourceSelect = document.querySelector('#source-select');
const refreshSourcesButton = document.querySelector('#refresh-sources');
const sourceAuthBadge = document.querySelector('#source-auth-badge');
const sourceAuthMeta = document.querySelector('#source-auth-meta');
const sourceLoginButton = document.querySelector('#source-login-button');
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
const TASK_KIND_LABELS = {
  download: '下载',
  export: '导出',
};
const TASK_STAGE_LABELS = {
  queued: '等待开始',
  started: '已启动',
  chapter_progress: '逐章下载中',
  batch_progress: '批量下载中',
  prepare: '收集章节中',
  write: '写入文件中',
  finished: '已完成',
  failed: '失败',
};
const taskElements = new Map();

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

function escapeHtml(value) {
  return String(value ?? '')
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;');
}

function formatWordCount(value) {
  const text = String(value ?? '').trim();
  if (!text) return '待补充';
  if (!/^\d+$/.test(text)) return text;

  const count = Number(text);
  if (!Number.isFinite(count) || count <= 0) return '待补充';
  if (count >= 100000000) return `${(count / 100000000).toFixed(1)} 亿字`;
  if (count >= 10000) return `${(count / 10000).toFixed(1)} 万字`;
  return `${count.toLocaleString('zh-CN')} 字`;
}

function formatScore(value) {
  const score = Number(value);
  if (!Number.isFinite(score) || score <= 0) return '未评分';
  return score.toFixed(1);
}

function formatCreationStatus(value) {
  return Number(value) === 1 ? '已完结' : '连载中';
}

function formatGender(value) {
  if (Number(value) === 1) return '男频';
  if (Number(value) === 2) return '女频';
  return '通用';
}

function formatTimestamp(value) {
  const timestamp = Number(value);
  if (!Number.isFinite(timestamp) || timestamp <= 0) return '待同步';

  const date = new Date(timestamp * 1000);
  if (Number.isNaN(date.getTime())) return '待同步';

  return new Intl.DateTimeFormat('zh-CN', {
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
  }).format(date);
}

function getCurrentSourceName() {
  return sourceSelect?.selectedOptions?.[0]?.textContent?.trim() || '当前书源';
}

function getCurrentDownloadMode() {
  if (!state.sourceCapabilities) {
    return '下载方式待定';
  }
  if (state.sourceCapabilities.batch_enabled) {
    return '批量下载';
  }
  if (state.sourceCapabilities.supports_batch) {
    return '逐章下载（批量待登录）';
  }
  return '逐章下载';
}

function getCurrentLoginStatusText() {
  if (!state.sourceCapabilities) {
    return '状态读取中';
  }
  if (!state.sourceCapabilities.supports_login) {
    return '无需登录';
  }
  return state.sourceCapabilities.logged_in ? '已登录' : '未登录';
}

function renderSourceAuth() {
  if (!sourceAuthBadge || !sourceAuthMeta || !sourceLoginButton) return;

  const capabilities = state.sourceCapabilities;
  const sourceName = getCurrentSourceName();

  if (!capabilities) {
    sourceAuthBadge.textContent = '状态读取中';
    sourceAuthBadge.className = 'source-auth-badge is-neutral';
    sourceAuthMeta.textContent = '正在读取当前书源登录状态';
    sourceLoginButton.style.display = 'none';
    return;
  }

  sourceAuthBadge.textContent = getCurrentLoginStatusText();
  if (!capabilities.supports_login) {
    sourceAuthBadge.className = 'source-auth-badge is-neutral';
    sourceAuthMeta.textContent = `${sourceName} 不需要登录，当前使用 ${getCurrentDownloadMode()}`;
    sourceLoginButton.style.display = 'none';
    return;
  }

  sourceAuthBadge.className = `source-auth-badge ${capabilities.logged_in ? 'is-online' : 'is-offline'}`;
  sourceLoginButton.style.display = 'inline-flex';
  sourceLoginButton.textContent = capabilities.logged_in ? '重新登录' : '登录';

  if (capabilities.logged_in) {
    sourceAuthMeta.textContent = `${sourceName} 当前会话已登录，${getCurrentDownloadMode()} 已启用`;
    return;
  }

  if (capabilities.supports_batch) {
    sourceAuthMeta.textContent = `${sourceName} 当前未登录，下载会回退为逐章模式；登录后可启用批量下载`;
    return;
  }

  sourceAuthMeta.textContent = `${sourceName} 支持会话登录，但当前功能无需登录即可使用`;
}

function getBookMonogram(book) {
  const text = String(book?.title || book?.author || '书').trim().replace(/\s+/g, '');
  return escapeHtml(text.slice(0, 2) || '书');
}

function renderCoverFallback(book) {
  return `
    <div class="book-cover-placeholder">
      <span class="book-cover-placeholder-mark">${getBookMonogram(book)}</span>
      <span class="book-cover-placeholder-text">BOOK PROFILE</span>
    </div>
  `;
}

function renderTaskCover(detail) {
  const coverUrl = String(detail?.cover_url || '').trim();
  if (coverUrl) {
    return `<img class="task-cover-image" src="${escapeHtml(coverUrl)}" alt="${escapeHtml(detail?.title || detail?.book_id || '任务')} 封面">`;
  }
  return `<div class="task-cover-fallback">${getBookMonogram(detail)}</div>`;
}

function attachTaskCoverFallback(coverNode, detail) {
  const image = coverNode.querySelector('.task-cover-image');
  if (!image) return;

  image.addEventListener('error', () => {
    coverNode.innerHTML = `<div class="task-cover-fallback">${getBookMonogram(detail)}</div>`;
  }, { once: true });
}

function renderBookDetail(book) {
  if (!book || !detailView) return;

  const title = escapeHtml(book.title || '未命名书籍');
  const author = escapeHtml(book.author || '未知作者');
  const category = escapeHtml(book.category || '未分类');
  const abstract = escapeHtml(book.abstract || '暂无简介');
  const latestChapter = escapeHtml(book.last_chapter_title || '暂未获取章节信息');
  const sourceName = escapeHtml(getCurrentSourceName());
  const wordCount = escapeHtml(formatWordCount(book.word_count));
  const score = escapeHtml(formatScore(book.score));
  const status = formatCreationStatus(book.creation_status);
  const statusClass = Number(book.creation_status) === 1 ? 'is-completed' : 'is-serial';
  const gender = escapeHtml(formatGender(book.gender));
  const updateTime = escapeHtml(formatTimestamp(book.last_update_time));
  const bookId = escapeHtml(book.book_id || '-');
  const coverUrl = String(book.cover_url || '').trim();
  const safeCoverUrl = escapeHtml(coverUrl);

  detailView.className = 'book-detail';
  detailView.innerHTML = `
    <div class="book-detail-hero">
      <div class="book-cover ${coverUrl ? 'has-image' : 'is-fallback'}" aria-hidden="true">
        ${coverUrl ? `
          <img class="book-cover-image" src="${safeCoverUrl}" alt="${title} 封面">
        ` : `
          ${renderCoverFallback(book)}
        `}
      </div>

      <div class="book-detail-main">
        <div class="book-detail-kicker">作品档案</div>
        <div class="book-detail-header">
          <div class="book-detail-heading">
            <div class="title">${title}</div>
            <div class="book-detail-author">作者 · ${author}</div>
          </div>
        </div>
      </div>

      <div class="detail-pill-row">
        <span class="detail-status-badge ${statusClass}">${escapeHtml(status)}</span>
        <span class="detail-pill">${sourceName}</span>
        <span class="detail-pill">${escapeHtml(getCurrentDownloadMode())}</span>
        <span class="detail-pill">${category}</span>
        <span class="detail-pill">${gender}</span>
      </div>

      <div class="detail-stat-grid">
        <div class="detail-stat">
          <span class="detail-stat-label">字数规模</span>
          <strong>${wordCount}</strong>
        </div>
        <div class="detail-stat">
          <span class="detail-stat-label">站内评分</span>
          <strong>${score}</strong>
        </div>
        <div class="detail-stat">
          <span class="detail-stat-label">更新日期</span>
          <strong>${updateTime}</strong>
        </div>
      </div>
    </div>

    <section class="detail-facts">
      <div class="detail-fact">
        <span class="detail-fact-label">最新章节</span>
        <strong>${latestChapter}</strong>
      </div>
      <div class="detail-fact">
        <span class="detail-fact-label">书籍编号</span>
        <strong class="detail-fact-id">${bookId}</strong>
      </div>
    </section>

    <section class="detail-section">
      <div class="detail-section-head">内容简介</div>
      <p class="detail-section-body">${abstract}</p>
    </section>
  `;

  const coverImage = detailView.querySelector('.book-cover-image');
  if (coverImage) {
    coverImage.addEventListener('error', () => {
      const coverContainer = coverImage.closest('.book-cover');
      if (!coverContainer) return;
      coverContainer.classList.remove('has-image');
      coverContainer.classList.add('is-fallback');
      coverContainer.innerHTML = renderCoverFallback(book);
    }, { once: true });
  }
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

function hasTask(taskId) {
  return state.tasks.some(task => task.task_id === taskId);
}

function formatTaskKind(kind) {
  return TASK_KIND_LABELS[kind] || String(kind || '任务');
}

function formatTaskStage(stage) {
  return TASK_STAGE_LABELS[stage] || String(stage || '处理中');
}

function getTaskStateClass(detail) {
  if (detail.stage === 'failed') return 'is-failed';
  if (detail.stage === 'finished') return 'is-finished';
  if (detail.stage === 'queued') return 'is-queued';
  return 'is-active';
}

function renderTasks() {
  if (!taskFeed) return;

  if (!state.tasks.length) {
    if (!taskFeed.querySelector('.empty-state')) {
      taskFeed.innerHTML = `<div class="empty-state" style="padding: 24px;">当前没有进行中的任务</div>`;
    }
  } else {
    const emptyState = taskFeed.querySelector('.empty-state');
    if (emptyState) {
      emptyState.remove();
    }
  }
}

function createTaskElement(detail) {
  const item = document.createElement('div');
  item.innerHTML = `
    <div class="task-layout">
      <div class="task-cover"></div>
      <div class="task-content">
        <strong class="task-heading"></strong>
        <div class="task-title"></div>
        <div class="task-subtitle"></div>
        <div class="meta task-meta"></div>
      </div>
    </div>
  `;

  item.taskRefs = {
    cover: item.querySelector('.task-cover'),
    heading: item.querySelector('.task-heading'),
    title: item.querySelector('.task-title'),
    subtitle: item.querySelector('.task-subtitle'),
    meta: item.querySelector('.task-meta'),
  };
  updateTaskElement(item, detail);
  return item;
}

function updateTaskElement(item, detail) {
  const refs = item.taskRefs;
  item.className = `task-item ${getTaskStateClass(detail)}`;

  const progress = Number.isFinite(detail.current) && Number.isFinite(detail.total)
    ? `${detail.current}/${detail.total}`
    : '';
  const title = detail.title || detail.book_id || '任务进行中';
  const subtitle = detail.book_id || '';
  const meta = detail.error_message
    || progress
    || (detail.format ? `格式：${String(detail.format).toUpperCase()}` : '')
    || (detail.optimistic ? '等待开始...' : '');
  const coverMarkup = renderTaskCover(detail);

  refs.heading.textContent = `${formatTaskKind(detail.kind)} · ${formatTaskStage(detail.stage)}`;
  refs.title.textContent = title;
  refs.subtitle.textContent = subtitle;
  refs.subtitle.style.display = subtitle ? '' : 'none';
  refs.meta.textContent = meta;
  refs.meta.style.display = meta ? '' : 'none';

  if (refs.cover.dataset.coverMarkup !== coverMarkup) {
    refs.cover.dataset.coverMarkup = coverMarkup;
    refs.cover.innerHTML = coverMarkup;
    attachTaskCoverFallback(refs.cover, detail);
  }
}

function findTaskById(taskId) {
  return state.tasks.find(task => task.task_id === taskId) || null;
}

function findOptimisticTask(detail) {
  if (!detail?.kind || !detail?.book_id) {
    return null;
  }
  return state.tasks.find((task) => (
    task.optimistic
    && task.kind === detail.kind
    && task.book_id === detail.book_id
  )) || null;
}

function upsertTask(detail) {
  if (!detail || typeof detail !== 'object') return;

  const taskId = String(detail.task_id || `${detail.kind || 'task'}-${Date.now()}`);
  let current = findTaskById(taskId);
  let previousTaskId = taskId;

  if (!current) {
    current = findOptimisticTask(detail);
    previousTaskId = current?.task_id || taskId;
  }

  if (current) {
    if (previousTaskId !== taskId) {
      const item = taskElements.get(previousTaskId);
      if (item) {
        taskElements.delete(previousTaskId);
        taskElements.set(taskId, item);
      }
    }

    Object.assign(current, detail, {
      task_id: taskId,
      optimistic: detail.optimistic ?? taskId.startsWith('pending-'),
      updated_at: Date.now(),
    });

    const item = taskElements.get(taskId);
    if (item) {
      updateTaskElement(item, current);
    }
    renderTasks();
    return;
  }

  const next = {
    ...detail,
    task_id: taskId,
    optimistic: detail.optimistic ?? taskId.startsWith('pending-'),
    updated_at: Date.now(),
  };
  const item = createTaskElement(next);

  state.tasks.unshift(next);
  taskElements.set(taskId, item);
  renderTasks();
  taskFeed.prepend(item);

  while (state.tasks.length > 20) {
    const removed = state.tasks.pop();
    const removedItem = taskElements.get(removed.task_id);
    if (removedItem) {
      removedItem.remove();
      taskElements.delete(removed.task_id);
    }
  }
}

function createOptimisticTask(kind, book, extra = {}) {
  const taskId = `pending-${kind}-${Date.now()}`;
  upsertTask({
    task_id: taskId,
    kind,
    stage: 'queued',
    book_id: book?.book_id || '',
    title: book?.title || '',
    cover_url: book?.cover_url || '',
    optimistic: true,
    ...extra,
  });
  return taskId;
}

function settleOptimisticTask(_optimisticTaskId, detail) {
  upsertTask({
    ...detail,
    optimistic: false,
  });
}

async function loadSources() {
  const payload = await callApp('get_sources');
  renderSources(payload);
  state.sourceCapabilities = await callApp('getSourceCapabilities');
  renderSourceAuth();
  if (state.selectedBook) {
    renderBookDetail(state.selectedBook);
  }
  const count = Array.isArray(payload?.sources) ? payload.sources.length : 0;
  setStatus(`已加载 ${count} 个书源 · 当前${getCurrentDownloadMode()}`);
}

async function refreshSourceCapabilities() {
  state.sourceCapabilities = await callApp('getSourceCapabilities');
  renderSourceAuth();
  if (state.selectedBook) {
    renderBookDetail(state.selectedBook);
  }
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
        setStatus(`书源已刷新 · 当前${getCurrentDownloadMode()}`);
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
        await refreshSourceCapabilities();
        setStatus(`已切换书源: ${payload.source?.name || sourceSelect.value} · 当前${getCurrentDownloadMode()}`);
        await loadBookshelf();
    } catch (error) {
        showError(error);
    }
    });
}

if(sourceLoginButton) {
    sourceLoginButton.addEventListener('click', async () => {
    if (!state.sourceCapabilities?.supports_login) return;
    const btn = sourceLoginButton;
    btn.disabled = true;
    try {
        setStatus(`正在登录 ${getCurrentSourceName()}...`);
        await callApp('login');
        await refreshSourceCapabilities();
        setStatus(`已登录 ${getCurrentSourceName()} · 当前${getCurrentDownloadMode()}`);
    } catch (error) {
        await refreshSourceCapabilities();
        showError(error);
    } finally {
        btn.disabled = false;
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
    const optimisticTaskId = createOptimisticTask('download', state.selectedBook);
    btn.disabled = true;
    try {
        setStatus('开始下载任务...');
        switchPage('tasks-page');
        const payload = await callApp('download_book', state.selectedBook);
        settleOptimisticTask(optimisticTaskId, {
          task_id: payload.task_id,
          kind: 'download',
          stage: 'finished',
          book_id: state.selectedBook.book_id,
          title: state.selectedBook.title,
          cover_url: state.selectedBook.cover_url,
          current: payload.downloaded,
          total: payload.downloaded,
        });
        setStatus('缓存完成');

        const toc = await callApp('get_toc', state.selectedBook.book_id, false);
        renderToc(toc);
    } catch (error) {
        if (hasTask(optimisticTaskId)) {
          upsertTask({
            task_id: optimisticTaskId,
            kind: 'download',
            stage: 'failed',
            book_id: state.selectedBook.book_id,
            title: state.selectedBook.title,
            cover_url: state.selectedBook.cover_url,
            error_message: error?.error?.message || String(error),
            optimistic: false,
          });
        }
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
    const optimisticTaskId = createOptimisticTask('export', state.selectedBook, {
      format: exportFormat.value,
    });
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
        settleOptimisticTask(optimisticTaskId, {
          task_id: payload.task_id,
          kind: 'export',
          stage: 'finished',
          book_id: state.selectedBook.book_id,
          title: state.selectedBook.title,
          cover_url: state.selectedBook.cover_url,
          format: exportFormat.value,
          path: payload.path,
        });
        if(exportsDir) exportsDir.textContent = payload.exports_dir;
        setStatus('导出完成');
    } catch (error) {
        if (hasTask(optimisticTaskId)) {
          upsertTask({
            task_id: optimisticTaskId,
            kind: 'export',
            stage: 'failed',
            book_id: state.selectedBook.book_id,
            title: state.selectedBook.title,
            cover_url: state.selectedBook.cover_url,
            format: exportFormat.value,
            error_message: error?.error?.message || String(error),
            optimistic: false,
          });
        }
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
  upsertTask(event.detail);
});

async function bootstrap() {
  try {
    renderTasks();
    await loadSources();
    await loadBookshelf();

    // Initialize navigation state
    switchPage('search-page');

    if(exportsDir) exportsDir.textContent = '程序运行目录';
    setStatus('系统就绪');
  } catch (error) {
    showError(error);
  }
}

bootstrap();

