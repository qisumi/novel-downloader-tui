const state = {
  selectedBook: null,
  toc: [],
  bookshelfItems: [],
  tasks: [],
  currentPage: 'search-page',
  isDetailVisible: false,
  sourceCapabilities: null
};

const navItems = document.querySelectorAll('.nav-item');
const pageViews = document.querySelectorAll('.page-view');
const detailPage = document.getElementById('detail-page');
const backButton = document.getElementById('back-button');
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
const TASK_KIND_LABELS = { download: '下载', export: '导出' };
const TASK_STAGE_LABELS = {
  queued: '等待开始', started: '已启动',
  chapter_progress: '逐章下载中', batch_progress: '批量下载中',
  prepare: '收集章节中', write: '写入文件中',
  finished: '已完成', failed: '失败',
};

function switchPage(targetId) {
  state.currentPage = targetId;
  if (state.isDetailVisible) { hideDetailPage(); }
  navItems.forEach(item => {
    item.classList.toggle("active", item.dataset.target === targetId);
  });
  syncPageVisibility();
}

function syncPageVisibility() {
  pageViews.forEach(page => {
    if (page.id === 'detail-page') return;
    page.classList.remove('detail-active');
    page.style.display = '';
    page.classList.toggle('active', page.id === state.currentPage);
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

navItems.forEach(item => {
  item.addEventListener('click', () => switchPage(item.dataset.target));
});
if (backButton) backButton.addEventListener('click', hideDetailPage);

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
  if (result?.ok === false) throw result;
  return result && typeof result === 'object' && 'data' in result ? result.data : result;
}

function withButtonLock(btn, handler) {
  if (!btn) return;
  btn.addEventListener('click', async () => {
    btn.disabled = true;
    try { await handler(); }
    catch (error) { showError(error); }
    finally { btn.disabled = false; }
  });
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
    const abstractHtml = book.abstract
      ? `<div style="margin-top: 4px; font-size: 0.9rem; color: var(--text); display: -webkit-box; -webkit-line-clamp: 2; -webkit-box-orient: vertical; overflow: hidden;">${book.abstract}</div>`
      : '';
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
  saveBookshelfBtn.style.display = inBookshelf ? 'none' : 'block';
  removeBookshelfBtn.style.display = inBookshelf ? 'block' : 'none';
}

function escapeHtml(value) {
  return String(value ?? '')
    .replace(/&/g, '&amp;').replace(/</g, '&lt;')
    .replace(/>/g, '&gt;').replace(/"/g, '&quot;').replace(/'/g, '&#39;');
}

function normalizeCoverUrl(value) {
  return String(value ?? '').trim();
}

function isDisplayableCoverUrl(value) {
  const url = normalizeCoverUrl(value);
  return !!url && !/\.hei[cf](?:$|[?#])/i.test(url);
}

function pickDisplayableCoverUrl(...values) {
  for (const value of values) {
    const url = normalizeCoverUrl(value);
    if (isDisplayableCoverUrl(url)) {
      return url;
    }
  }
  return '';
}

function mergeBookDetail(currentBook, detailBook) {
  const merged = { ...(currentBook || {}), ...(detailBook || {}) };
  merged.cover_url = pickDisplayableCoverUrl(detailBook?.cover_url, currentBook?.cover_url);
  return merged;
}

function formatWordCount(value) {
  const text = String(value ?? '').trim();
  if (!text || !/^\d+$/.test(text)) return text || '待补充';
  const count = Number(text);
  if (!Number.isFinite(count) || count <= 0) return '待补充';
  if (count >= 100000000) return `${(count / 100000000).toFixed(1)} 亿字`;
  if (count >= 10000) return `${(count / 10000).toFixed(1)} 万字`;
  return `${count.toLocaleString('zh-CN')} 字`;
}

function formatScore(v) { const s = Number(v); return Number.isFinite(s) && s > 0 ? s.toFixed(1) : '未评分'; }
function formatCreationStatus(v) { return Number(v) === 1 ? '已完结' : '连载中'; }
function formatGender(v) { return Number(v) === 1 ? '男频' : Number(v) === 2 ? '女频' : '通用'; }
function formatTimestamp(value) {
  const ts = Number(value);
  if (!Number.isFinite(ts) || ts <= 0) return '待同步';
  const date = new Date(ts * 1000);
  return Number.isNaN(date.getTime()) ? '待同步' : new Intl.DateTimeFormat('zh-CN', { year:'numeric', month:'2-digit', day:'2-digit' }).format(date);
}

function getCurrentSourceName() { return sourceSelect?.selectedOptions?.[0]?.textContent?.trim() || '当前书源'; }
function getCurrentDownloadMode() {
  if (!state.sourceCapabilities) return '下载方式待定';
  if (state.sourceCapabilities.batch_enabled) return '批量下载';
  if (state.sourceCapabilities.supports_batch) return '逐章下载（批量待登录）';
  return '逐章下载';
}
function getCurrentLoginStatusText() {
  if (!state.sourceCapabilities) return '状态读取中';
  if (!state.sourceCapabilities.supports_login) return '无需登录';
  return state.sourceCapabilities.logged_in ? '已登录' : '未登录';
}

function renderSourceAuth() {
  if (!sourceAuthBadge || !sourceAuthMeta || !sourceLoginButton) return;
  const cap = state.sourceCapabilities;
  const name = getCurrentSourceName();
  if (!cap) {
    sourceAuthBadge.textContent = '状态读取中';
    sourceAuthBadge.className = 'source-auth-badge is-neutral';
    sourceAuthMeta.textContent = '正在读取当前书源登录状态';
    sourceLoginButton.style.display = 'none';
    return;
  }
  sourceAuthBadge.textContent = getCurrentLoginStatusText();
  if (!cap.supports_login) {
    sourceAuthBadge.className = 'source-auth-badge is-neutral';
    sourceAuthMeta.textContent = `${name} 不需要登录，当前使用 ${getCurrentDownloadMode()}`;
    sourceLoginButton.style.display = 'none';
    return;
  }
  sourceAuthBadge.className = `source-auth-badge ${cap.logged_in ? 'is-online' : 'is-offline'}`;
  sourceLoginButton.style.display = 'inline-flex';
  sourceLoginButton.textContent = cap.logged_in ? '重新登录' : '登录';
  sourceAuthMeta.textContent = cap.logged_in
    ? `${name} 当前会话已登录，${getCurrentDownloadMode()} 已启用`
    : cap.supports_batch
      ? `${name} 当前未登录，下载会回退为逐章模式；登录后可启用批量下载`
      : `${name} 支持会话登录，但当前功能无需登录即可使用`;
}

function getBookMonogram(book) {
  return escapeHtml(String(book?.title || book?.author || '书').trim().replace(/\s+/g, '').slice(0, 2) || '书');
}

function renderCoverFallback(book) {
  return `<div class="book-cover-placeholder"><span class="book-cover-placeholder-mark">${getBookMonogram(book)}</span><span class="book-cover-placeholder-text">BOOK PROFILE</span></div>`;
}

function createTaskCoverFallbackElement(detail) {
  const fallback = document.createElement('div');
  fallback.className = 'task-cover-fallback';
  fallback.textContent = getBookMonogram(detail);
  return fallback;
}

function renderBookDetail(book) {
  if (!book || !detailView) return;
  const e = escapeHtml;
  const coverUrl = pickDisplayableCoverUrl(book.cover_url);
  detailView.className = 'book-detail';
  detailView.innerHTML = `
    <div class="book-detail-hero">
      <div class="book-cover ${coverUrl ? 'has-image' : 'is-fallback'}" aria-hidden="true">
        ${coverUrl ? `<img class="book-cover-image" src="${e(coverUrl)}" alt="${e(book.title)} 封面">` : renderCoverFallback(book)}
      </div>
      <div class="book-detail-main">
        <div class="book-detail-kicker">作品档案</div>
        <div class="book-detail-header"><div class="book-detail-heading">
          <div class="title">${e(book.title || '未命名书籍')}</div>
          <div class="book-detail-author">作者 · ${e(book.author || '未知作者')}</div>
        </div></div>
      </div>
      <div class="detail-pill-row">
        <span class="detail-status-badge ${Number(book.creation_status) === 1 ? 'is-completed' : 'is-serial'}">${e(formatCreationStatus(book.creation_status))}</span>
        <span class="detail-pill">${e(getCurrentSourceName())}</span>
        <span class="detail-pill">${e(getCurrentDownloadMode())}</span>
        <span class="detail-pill">${e(book.category || '未分类')}</span>
        <span class="detail-pill">${e(formatGender(book.gender))}</span>
      </div>
      <div class="detail-stat-grid">
        <div class="detail-stat"><span class="detail-stat-label">字数规模</span><strong>${e(formatWordCount(book.word_count))}</strong></div>
        <div class="detail-stat"><span class="detail-stat-label">站内评分</span><strong>${e(formatScore(book.score))}</strong></div>
        <div class="detail-stat"><span class="detail-stat-label">更新日期</span><strong>${e(formatTimestamp(book.last_update_time))}</strong></div>
      </div>
    </div>
    <section class="detail-facts">
      <div class="detail-fact"><span class="detail-fact-label">最新章节</span><strong>${e(book.last_chapter_title || '暂未获取章节信息')}</strong></div>
      <div class="detail-fact"><span class="detail-fact-label">书籍编号</span><strong class="detail-fact-id">${e(book.book_id || '-')}</strong></div>
    </section>
    <section class="detail-section">
      <div class="detail-section-head">内容简介</div>
      <p class="detail-section-body">${e(book.abstract || '暂无简介')}</p>
    </section>`;
  const coverImage = detailView.querySelector('.book-cover-image');
  if (coverImage) {
    coverImage.addEventListener('error', () => {
      const c = coverImage.closest('.book-cover');
      if (c) { c.classList.remove('has-image'); c.classList.add('is-fallback'); c.innerHTML = renderCoverFallback(book); }
    }, { once: true });
  }
}

function renderToc(payload) {
  state.toc = payload.items;
  if (tocSummary) tocSummary.textContent = `共 ${payload.toc_count || payload.items.length} 章，已缓存 ${payload.cached_chapter_count || 0} 章`;
  if (exportStart) exportStart.max = String(Math.max(1, payload.items.length));
  if (exportEnd) { exportEnd.max = String(Math.max(1, payload.items.length)); exportEnd.value = exportEnd.max; }
  if (!tocList) return;
  if (!payload.items.length) { tocList.innerHTML = '<div class="empty-state">暂无目录数据</div>'; return; }
  tocList.innerHTML = payload.items.map((item, i) => `
    <div class="toc-item ${item.cached ? 'cached' : ''}">
      <strong>${String(i + 1).padStart(3, "0")} · ${item.title || "未命名章节"}</strong>
      <div class="meta">${item.volume_name || "默认卷"} · ${item.word_count || 0} 字${item.cached ? " · 已缓存" : ""}</div>
    </div>`).join('');
}

// ── Task management (simplified) ─────────────

const taskElements = new Map();

function getTaskStateClass(stage) {
  if (stage === 'failed') return 'is-failed';
  if (stage === 'finished') return 'is-finished';
  if (stage === 'queued') return 'is-queued';
  return 'is-active';
}

function getTaskHeading(detail) {
  return `${TASK_KIND_LABELS[detail.kind] || detail.kind || '任务'} · ${TASK_STAGE_LABELS[detail.stage] || detail.stage || '处理中'}`;
}

function getTaskMeta(detail) {
  const progress = Number.isFinite(detail.current) && Number.isFinite(detail.total) ? `${detail.current}/${detail.total}` : '';
  return detail.error_message || progress || (detail.format ? `格式：${String(detail.format).toUpperCase()}` : '') || (detail.optimistic ? '等待开始...' : '');
}

function getTaskTitle(detail) {
  return String(detail.title || detail.book_id || '任务进行中');
}

function createTaskEmptyState() {
  const empty = document.createElement('div');
  empty.className = 'empty-state';
  empty.style.padding = '24px';
  empty.textContent = '当前没有进行中的任务';
  return empty;
}

function createTaskCoverImage(detail, coverUrl) {
  const image = document.createElement('img');
  image.className = 'task-cover-image';
  image.alt = '封面';
  image.src = coverUrl;
  image.dataset.title = getTaskTitle(detail);
  image.dataset.author = String(detail.author || '');
  image.addEventListener('error', () => {
    const container = image.closest('.task-cover');
    if (!container) return;
    container.classList.add('is-fallback');
    container.replaceChildren(createTaskCoverFallbackElement({
      title: image.dataset.title || '',
      author: image.dataset.author || '',
    }));
  }, { once: true });
  return image;
}

function updateTaskCover(container, detail) {
  if (!container) return;
  const coverUrl = pickDisplayableCoverUrl(detail.cover_url);
  const title = getTaskTitle(detail);
  const author = String(detail.author || '');
  const currentImage = container.querySelector('.task-cover-image');
  const currentFallback = container.querySelector('.task-cover-fallback');

  if (!coverUrl) {
    container.classList.add('is-fallback');
    if (!currentFallback || currentImage || currentFallback.textContent !== getBookMonogram(detail)) {
      container.replaceChildren(createTaskCoverFallbackElement(detail));
    }
    return;
  }

  container.classList.remove('is-fallback');
  if (currentImage) {
    if (currentImage.getAttribute('src') !== coverUrl) currentImage.setAttribute('src', coverUrl);
    if (currentImage.dataset.title !== title) currentImage.dataset.title = title;
    if (currentImage.dataset.author !== author) currentImage.dataset.author = author;
    return;
  }

  container.replaceChildren(createTaskCoverImage(detail, coverUrl));
}

function createTaskElement(detail) {
  const item = document.createElement('div');
  item.innerHTML = `<div class="task-layout">
    <div class="task-cover"></div>
    <div class="task-content">
      <strong class="task-heading"></strong>
      <div class="task-title"></div>
      <div class="task-subtitle"></div>
      <div class="meta task-meta"></div>
    </div></div>`;
  updateTaskElement(item, detail);
  return item;
}

function updateTaskElement(item, detail) {
  if (!item) return;
  item.className = `task-item ${getTaskStateClass(detail.stage)}`;
  item.dataset.taskId = String(detail.task_id || '');

  const heading = item.querySelector('.task-heading');
  const title = item.querySelector('.task-title');
  const subtitle = item.querySelector('.task-subtitle');
  const meta = item.querySelector('.task-meta');

  if (heading) heading.textContent = getTaskHeading(detail);
  if (title) title.textContent = getTaskTitle(detail);
  if (subtitle) {
    const subtitleText = String(detail.book_id || '');
    subtitle.textContent = subtitleText;
    subtitle.hidden = !subtitleText;
  }
  if (meta) {
    const metaText = getTaskMeta(detail);
    meta.textContent = metaText;
    meta.hidden = !metaText;
  }

  updateTaskCover(item.querySelector('.task-cover'), detail);
}

function findTaskElement(taskId) {
  return taskElements.get(String(taskId || '')) || null;
}

function syncTaskElement(detail, previousTaskId = '') {
  if (!taskFeed) return;

  const taskId = String(detail.task_id || '');
  const oldTaskId = String(previousTaskId || '');
  let item = (oldTaskId && findTaskElement(oldTaskId)) || findTaskElement(taskId);

  if (!item) {
    item = createTaskElement(detail);
    taskElements.set(taskId, item);
    const emptyState = taskFeed.querySelector('.empty-state');
    if (emptyState) {
      taskFeed.replaceChildren(item);
    } else {
      taskFeed.prepend(item);
    }
    return;
  }

  updateTaskElement(item, detail);
  if (oldTaskId && oldTaskId !== taskId) {
    taskElements.delete(oldTaskId);
  }
  taskElements.set(taskId, item);
}

function removeTaskElement(taskId) {
  const key = String(taskId || '');
  const item = findTaskElement(key);
  if (item) item.remove();
  taskElements.delete(key);
  if (taskFeed && !taskFeed.querySelector('.task-item')) {
    taskFeed.replaceChildren(createTaskEmptyState());
  }
}

function renderAllTasks() {
  if (!taskFeed) return;
  taskElements.clear();
  if (!state.tasks.length) {
    taskFeed.replaceChildren(createTaskEmptyState());
    return;
  }
  const fragment = document.createDocumentFragment();
  state.tasks.forEach((task) => {
    const item = createTaskElement(task);
    taskElements.set(String(task.task_id || ''), item);
    fragment.append(item);
  });
  taskFeed.replaceChildren(fragment);
}

function findTask(predicate) {
  for (let i = 0; i < state.tasks.length; i++) {
    if (predicate(state.tasks[i], i)) return i;
  }
  return -1;
}

function upsertTask(detail) {
  if (!detail || typeof detail !== 'object') return;
  const taskId = String(detail.task_id || `${detail.kind || 'task'}-${Date.now()}`);
  let idx = findTask((t) => t.task_id === taskId);
  if (idx < 0) idx = findTask((t) => t.optimistic && t.kind === detail.kind && t.book_id === detail.book_id);
  const current = idx >= 0 ? state.tasks[idx] : null;
  const previousTaskId = current?.task_id ? String(current.task_id) : '';
  const merged = {
    ...(current || {}),
    ...detail,
    task_id: taskId,
    optimistic: detail.optimistic ?? taskId.startsWith('pending-'),
    updated_at: Date.now(),
  };
  merged.cover_url = pickDisplayableCoverUrl(detail.cover_url, current?.cover_url);
  if (idx >= 0) {
    state.tasks[idx] = merged;
  } else {
    state.tasks.unshift(merged);
  }
  while (state.tasks.length > 20) {
    const removed = state.tasks.pop();
    removeTaskElement(removed?.task_id);
  }
  syncTaskElement(merged, previousTaskId);
}

function createOptimisticTask(kind, book, extra = {}) {
  const taskId = `pending-${kind}-${Date.now()}`;
  upsertTask({ task_id: taskId, kind, stage: 'queued', book_id: book?.book_id || '', title: book?.title || '', cover_url: book?.cover_url || '', optimistic: true, ...extra });
  return taskId;
}

// ── App logic ─────────────

async function loadSources() {
  const payload = await callApp('get_sources');
  renderSources(payload);
  state.sourceCapabilities = await callApp('getSourceCapabilities');
  renderSourceAuth();
  if (state.selectedBook) renderBookDetail(state.selectedBook);
  setStatus(`已加载 ${Array.isArray(payload?.sources) ? payload.sources.length : 0} 个书源 · 当前${getCurrentDownloadMode()}`);
}

async function refreshSourceCapabilities() {
  state.sourceCapabilities = await callApp('getSourceCapabilities');
  renderSourceAuth();
  if (state.selectedBook) renderBookDetail(state.selectedBook);
}

async function loadBookshelf() {
  state.bookshelfItems = (await callApp('list_bookshelf')).items || [];
  renderBooks(state.bookshelfItems, bookshelfList, '书架空空如也，去探索一下吧~');
  if (state.selectedBook) updateBookshelfButtons(state.selectedBook.book_id);
}

async function selectBook(book) {
  state.selectedBook = { ...book, cover_url: pickDisplayableCoverUrl(book?.cover_url) };
  showDetailPage();
  renderBookDetail(state.selectedBook);
  updateBookshelfButtons(state.selectedBook.book_id);
  if (tocSummary) tocSummary.textContent = '加载目录中...';
  if (tocList) tocList.innerHTML = '<div class="empty-state">正在同步书籍信息...</div>';
  setStatus(`正在加载《${book.title || book.book_id}》...`);
  try {
    const detail = await callApp('get_book_detail', book.book_id);
    state.selectedBook = mergeBookDetail(state.selectedBook, detail.book);
    renderBookDetail(state.selectedBook);
    updateBookshelfButtons(state.selectedBook.book_id);
    renderToc(await callApp('get_toc', state.selectedBook.book_id, false));
    setStatus(`已载入《${state.selectedBook.title || state.selectedBook.book_id}》`);
  } catch (error) {
    showError(error);
    if (tocList) tocList.innerHTML = '<div class="empty-state" style="color:#d32f2f;">加载目录失败</div>';
  }
}

if (searchForm) {
  searchForm.addEventListener('submit', async (event) => {
    event.preventDefault();
    const keywords = searchInput.value.trim();
    if (!keywords) { setStatus('请输入关键词', true); return; }
    setStatus(`正在搜索: ${keywords}...`);
    const submitBtn = searchForm.querySelector('button');
    submitBtn.disabled = true;
    submitBtn.textContent = '搜索中';
    searchResults.innerHTML = '<div class="empty-state" style="grid-column: 1 / -1;">正在检索，请稍候...</div>';
    try {
      renderBooks((await callApp('search_books', keywords, 0)).items, searchResults, '没有找到相关结果，换个词试试？');
      setStatus(`搜索完成，共找到 ${searchResults.querySelectorAll('.book-card-button').length} 本书籍`);
    } catch (error) {
      showError(error);
      searchResults.innerHTML = '<div class="empty-state" style="grid-column: 1 / -1; color:#d32f2f;">搜索失败</div>';
    } finally {
      submitBtn.disabled = false;
      submitBtn.textContent = '立即搜索';
    }
  });
}

withButtonLock(refreshSourcesButton, async () => {
  await loadSources();
  setStatus(`书源已刷新 · 当前${getCurrentDownloadMode()}`);
});

if (sourceSelect) {
  sourceSelect.addEventListener('change', async () => {
    try {
      const payload = await callApp('select_source', sourceSelect.value);
      await refreshSourceCapabilities();
      setStatus(`已切换书源: ${payload.source?.name || sourceSelect.value} · 当前${getCurrentDownloadMode()}`);
      await loadBookshelf();
    } catch (error) { showError(error); }
  });
}

withButtonLock(sourceLoginButton, async () => {
  if (!state.sourceCapabilities?.supports_login) return;
  setStatus(`正在登录 ${getCurrentSourceName()}...`);
  await callApp('login');
  await refreshSourceCapabilities();
  setStatus(`已登录 ${getCurrentSourceName()} · 当前${getCurrentDownloadMode()}`);
});

withButtonLock(document.querySelector('#reload-toc'), async () => {
  if (!state.selectedBook) return;
  setStatus('正在获取最新目录...');
  renderToc(await callApp('get_toc', state.selectedBook.book_id, true));
  setStatus('目录已更新到最新');
});

withButtonLock(document.querySelector('#download-book'), async () => {
  if (!state.selectedBook) return;
  const optimisticTaskId = createOptimisticTask('download', state.selectedBook);
  setStatus('开始下载任务...');
  switchPage('tasks-page');
  try {
    const payload = await callApp('download_book', state.selectedBook);
    upsertTask({ task_id: payload.task_id, kind: 'download', stage: 'finished',
      book_id: state.selectedBook.book_id, title: state.selectedBook.title,
      cover_url: state.selectedBook.cover_url, current: payload.downloaded, total: payload.downloaded });
    setStatus('缓存完成');
    renderToc(await callApp('get_toc', state.selectedBook.book_id, false));
  } catch (error) {
    upsertTask({ task_id: optimisticTaskId, kind: 'download', stage: 'failed',
      book_id: state.selectedBook.book_id, title: state.selectedBook.title,
      cover_url: state.selectedBook.cover_url,
      error_message: error?.error?.message || String(error), optimistic: false });
    throw error;
  }
});

withButtonLock(document.querySelector('#export-book'), async () => {
  if (!state.selectedBook) return;
  const start = Math.max(1, Number(exportStart.value || 1));
  const end = Math.max(start, Number(exportEnd.value || start));
  const optimisticTaskId = createOptimisticTask('export', state.selectedBook, { format: exportFormat.value });
  setStatus('正在导出，请稍候...');
  switchPage('tasks-page');
  try {
    const payload = await callApp('export_book', state.selectedBook, start - 1, end - 1, exportFormat.value);
    upsertTask({ task_id: payload.task_id, kind: 'export', stage: 'finished',
      book_id: state.selectedBook.book_id, title: state.selectedBook.title,
      cover_url: state.selectedBook.cover_url, format: exportFormat.value, path: payload.path });
    if (exportsDir) exportsDir.textContent = payload.exports_dir;
    setStatus('导出完成');
  } catch (error) {
    upsertTask({ task_id: optimisticTaskId, kind: 'export', stage: 'failed',
      book_id: state.selectedBook.book_id, title: state.selectedBook.title,
      cover_url: state.selectedBook.cover_url, format: exportFormat.value,
      error_message: error?.error?.message || String(error), optimistic: false });
    throw error;
  }
});

withButtonLock(saveBookshelfBtn, async () => {
  if (!state.selectedBook) return;
  await callApp('save_bookshelf', state.selectedBook);
  await loadBookshelf();
  setStatus('已成功加入书架');
});

withButtonLock(removeBookshelfBtn, async () => {
  if (!state.selectedBook) return;
  await callApp('remove_bookshelf', state.selectedBook.book_id);
  await loadBookshelf();
  setStatus('已从书架移除');
});

window.addEventListener('novel:task', (event) => upsertTask(event.detail));

async function bootstrap() {
  try {
    renderAllTasks();
    await loadSources();
    await loadBookshelf();
    switchPage('search-page');
    if (exportsDir) exportsDir.textContent = '程序运行目录';
    setStatus('系统就绪');
  } catch (error) { showError(error); }
}

bootstrap();
