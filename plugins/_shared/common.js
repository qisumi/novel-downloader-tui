const safeGet = (value, key, fallback) => {
  if (!value || typeof value !== "object") {
    return fallback;
  }
  if (!(key in value) || value[key] == null) {
    return fallback;
  }
  return value[key];
};

const scalarToString = (value) => {
  if (value == null) {
    return "";
  }
  return String(value);
};

const getString = (value, key, fallback = "") => scalarToString(safeGet(value, key, fallback));

const getNumber = (value, key, fallback = 0) => {
  const raw = safeGet(value, key, null);
  if (raw == null) {
    return fallback;
  }
  const parsed = Number(raw);
  return Number.isFinite(parsed) ? parsed : fallback;
};

const requireEnv = async (name) => {
  const value = (await host.env_get(name, "")) || "";
  if (!value) {
    host.config_error(`missing ${name}; please set it in the environment or .env`);
  }
  return value;
};

const appendQuery = (baseUrl, params) => {
  if (!params || typeof params !== "object") {
    return baseUrl;
  }

  const parts = [];
  for (const [key, value] of Object.entries(params)) {
    if (value == null || value === "") {
      continue;
    }
    parts.push(`${host.url_encode(String(key))}=${host.url_encode(scalarToString(value))}`);
  }

  if (parts.length === 0) {
    return baseUrl;
  }

  parts.sort();
  const separator = baseUrl.includes("?") ? "&" : "?";
  return `${baseUrl}${separator}${parts.join("&")}`;
};

const toFormBody = (params) => {
  if (!params || typeof params !== "object") {
    return "";
  }

  const parts = [];
  for (const [key, value] of Object.entries(params)) {
    if (value == null || value === "") {
      continue;
    }
    parts.push(`${host.url_encode(String(key))}=${host.url_encode(scalarToString(value))}`);
  }
  parts.sort();
  return parts.join("&");
};

const requestJson = async (request) => {
  const response = await host.http_request(request);
  if (response.status < 200 || response.status >= 300) {
    throw new Error(`request failed with status ${response.status}: ${scalarToString(request.url)}`);
  }

  try {
    return response.body ? JSON.parse(response.body) : {};
  } catch (error) {
    throw new Error(`__novel_data_error__:json_parse failed: ${error.message}`);
  }
};

const getJson = async (url, headers, timeoutSeconds) =>
  requestJson({
    method: "GET",
    url,
    headers,
    timeout_seconds: timeoutSeconds,
  });

const postFormJson = async (url, form, headers, timeoutSeconds) =>
  requestJson({
    method: "POST",
    url,
    headers: {
      "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8",
      ...(headers || {}),
    },
    body: toFormBody(form),
    timeout_seconds: timeoutSeconds,
  });

const postForm = async (url, form, headers, timeoutSeconds, followRedirects = true) =>
  host.http_request({
    method: "POST",
    url,
    headers: {
      "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8",
      ...(headers || {}),
    },
    body: toFormBody(form),
    timeout_seconds: timeoutSeconds,
    follow_redirects: followRedirects,
  });

module.exports = {
  safeGet,
  getString,
  getNumber,
  requireEnv,
  appendQuery,
  toFormBody,
  requestJson,
  getJson,
  postFormJson,
  postForm,
};
