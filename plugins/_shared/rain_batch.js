const common = require("./common");

const BATCH_PATH = "/web/batch.php";

const resolveBatchUrl = (baseUrl) => {
  try {
    return new URL(BATCH_PATH, String(baseUrl)).toString();
  } catch (_error) {
    const normalized = String(baseUrl || "").replace(/\/+$/, "");
    const origin = normalized.replace(/\/[^/]*$/, "");
    return `${origin}${BATCH_PATH}`;
  }
};

const requestBatch = async (baseUrl, payload) => {
  const root = await common.postFormJson(resolveBatchUrl(baseUrl), {
    ...(payload || {}),
  });

  if (root && typeof root.error === "string" && root.error.length > 0) {
    throw new Error(`batch request failed: ${root.error}`);
  }
  return root || {};
};

const normalizeChapter = (item) => ({
  item_id: common.getString(item, "item_id", common.getString(item, "id", "")),
  title: common.getString(item, "title", ""),
  content: common.getString(item, "content", ""),
});

const readBatchCount = (root) => {
  const count = Number(root?.batch_count ?? root?.batch ?? 0);
  if (!Number.isFinite(count) || count <= 0) {
    return 0;
  }
  return Math.trunc(count);
};

const getBatchCount = async (baseUrl, bookId) => {
  const root = await requestBatch(baseUrl, {
    id: bookId,
  });
  return readBatchCount(root);
};

const getBatch = async (baseUrl, bookId, batchNo) => {
  const root = await requestBatch(baseUrl, {
    id: bookId,
    batch: batchNo,
  });

  const items = root?.data;
  if (!Array.isArray(items)) {
    throw new Error(`unexpected batch payload: ${JSON.stringify(root)}`);
  }

  return items.map((item) => normalizeChapter(item));
};

module.exports = {
  getBatchCount,
  getBatch,
};
