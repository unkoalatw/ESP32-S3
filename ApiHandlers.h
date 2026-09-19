#ifndef API_HANDLERS_H
#define API_HANDLERS_H

#include "Config.h"
#include "SDCardManager.h"
#include "ConfigManager.h"
#include "WiFiManager.h"
#include "WeatherStation.h"
#include "IrRemoteManager.h"
#include "AudioRecorderManager.h"

// ---------------------------------------------------------------------------
// 存取日誌記錄結構 (Access Log Engine)
// ---------------------------------------------------------------------------
constexpr size_t MAX_ACCESS_LOGS = 25;
struct AccessLogEntry {
  uint32_t uptime;
  char ip[16];
  char method[8];
  char path[64];
  uint16_t status;
};

extern AccessLogEntry accessLogs[MAX_ACCESS_LOGS];
extern size_t accessLogHead;
extern size_t accessLogCount;

inline void recordAccessLog(uint16_t status) {
  accessLogs[accessLogHead].uptime = millis() / 1000;
  snprintf(accessLogs[accessLogHead].ip, sizeof(accessLogs[accessLogHead].ip), "%s", server.client().remoteIP().toString().c_str());
  snprintf(accessLogs[accessLogHead].method, sizeof(accessLogs[accessLogHead].method), "%s",
           (server.method() == HTTP_GET ? "GET" : (server.method() == HTTP_POST ? "POST" : "OTHER")));
  snprintf(accessLogs[accessLogHead].path, sizeof(accessLogs[accessLogHead].path), "%s", server.uri().c_str());
  accessLogs[accessLogHead].status = status;
  accessLogHead = (accessLogHead + 1) % MAX_ACCESS_LOGS;
  if (accessLogCount < MAX_ACCESS_LOGS) accessLogCount++;
}

inline void sendText(int code, const String &msg) {
  server.send(code, "text/plain; charset=utf-8", msg);
}

inline String urlDecode(const String &str) {
  String decoded = "";
  decoded.reserve(str.length());
  int len = str.length();
  for (int i = 0; i < len; ++i) {
    char c = str[i];
    if (c == '+') {
      decoded += ' ';
    } else if (c == '%' && i + 2 < len) {
      char hex[3] = { str[i + 1], str[i + 2], 0 };
      char val = static_cast<char>(strtoul(hex, nullptr, 16));
      decoded += val;
      i += 2;
    } else {
      decoded += c;
    }
  }
  return decoded;
}

// ---------------------------------------------------------------------------
// 健全鑑權與真隨機 Session 管理 (128-bit Random Token + RAM Session Table)
// ---------------------------------------------------------------------------
struct SessionEntry {
  char token[33];        // 32 位元 16 進位字串 + null 終結符
  uint32_t expiresAt;    // Unix 時間戳記或系統秒數 (到期時間)
  bool active;
};

constexpr size_t MAX_ACTIVE_SESSIONS = 8;
static SessionEntry g_activeSessions[MAX_ACTIVE_SESSIONS] = {};

inline String generateSecureSessionToken() {
  char hexBuf[33];
  uint32_t r1 = esp_random();
  uint32_t r2 = esp_random();
  uint32_t r3 = esp_random();
  uint32_t r4 = esp_random();
  snprintf(hexBuf, sizeof(hexBuf), "%08x%08x%08x%08x", (unsigned)r1, (unsigned)r2, (unsigned)r3, (unsigned)r4);
  return String(hexBuf);
}

inline String createNewSession() {
  uint32_t nowSec = millis() / 1000;
  String token = generateSecureSessionToken();

  // 尋找空位或已到期欄位
  int targetSlot = -1;
  for (size_t i = 0; i < MAX_ACTIVE_SESSIONS; i++) {
    if (!g_activeSessions[i].active || g_activeSessions[i].expiresAt < nowSec) {
      targetSlot = i;
      break;
    }
  }
  if (targetSlot == -1) targetSlot = 0; // 若全滿則替換最舊的第 0 格

  strncpy(g_activeSessions[targetSlot].token, token.c_str(), sizeof(g_activeSessions[targetSlot].token) - 1);
  g_activeSessions[targetSlot].token[sizeof(g_activeSessions[targetSlot].token) - 1] = '\0';
  g_activeSessions[targetSlot].expiresAt = nowSec + 604800; // 預設 7 天有效
  g_activeSessions[targetSlot].active = true;
  return token;
}

inline void invalidateSession(const String &token) {
  for (size_t i = 0; i < MAX_ACTIVE_SESSIONS; i++) {
    if (g_activeSessions[i].active && token == g_activeSessions[i].token) {
      g_activeSessions[i].active = false;
      g_activeSessions[i].token[0] = '\0';
    }
  }
}

inline bool isSessionTokenValid(const String &token) {
  if (token.length() != 32) return false;
  uint32_t nowSec = millis() / 1000;
  for (size_t i = 0; i < MAX_ACTIVE_SESSIONS; i++) {
    if (g_activeSessions[i].active && token == g_activeSessions[i].token) {
      if (g_activeSessions[i].expiresAt >= nowSec) {
        return true;
      } else {
        g_activeSessions[i].active = false; // 已過期
      }
    }
  }
  return false;
}

inline String extractSessionTokenFromCookie(const String &cookieHeader) {
  int idx = cookieHeader.indexOf("ESPSESSIONID=");
  if (idx == -1) return "";
  int start = idx + 13;
  int end = cookieHeader.indexOf(';', start);
  if (end == -1) end = cookieHeader.length();
  String token = cookieHeader.substring(start, end);
  token.trim();
  return token;
}

inline bool isClientAuthenticated() {
  // 若未設定任何密碼，視為公開存取模式
  if (runtimeWebPassword.length() == 0) {
    return true;
  }

  // 1. 檢查 Cookie 中的 ESPSESSIONID
  if (server.hasHeader("Cookie")) {
    String token = extractSessionTokenFromCookie(server.header("Cookie"));
    if (token.length() > 0 && isSessionTokenValid(token)) {
      return true;
    }
  }

  // 2. 檢查 HTTP Header X-Auth-Token
  if (server.hasHeader("X-Auth-Token")) {
    String token = server.header("X-Auth-Token");
    token.trim();
    if (token.length() > 0 && isSessionTokenValid(token)) {
      return true;
    }
  }

  return false;
}

inline bool requireAuth() {
  if (!isClientAuthenticated()) {
    recordAccessLog(401);
    server.send(401, "application/json; charset=utf-8", "{\"error\":\"unauthorized\",\"message\":\"請先登入系統\"}");
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// 前置宣告 (53 RESTful API Handlers)
// ---------------------------------------------------------------------------
inline void handleIndex();
inline void handleFavicon();
inline void handleManifestJson();
inline void handleServiceWorkerJs();
inline void handleLogin();
inline void handleLogout();
inline void handleAuthStatus();
inline void handleStatus();
inline void handleDiagnose();
inline void handleLogs();
inline void handleLogsClear();
inline void handleList();
inline void handleDownload();
inline void handleView();
inline void handleMkdir();
inline void handleRename();
inline void handleMove();
inline void handleDelete();
inline void handleUndo();
inline void handleEmptyTrash();
inline void handleUploadDone();
inline void handleUploadData();
inline void handleUploadStatus();
inline void handleUploadChunk();
inline void handleRead();
inline void handleWrite();
inline void handleUnzip();
inline void handleWiFiScan();
inline void handleWiFiConnect();
inline void handleWiFiDisconnect();
inline void handleWiFiStatus();
inline void handleRepeaterEnable();
inline void handleRepeaterDisable();
inline void handleRepeaterPost();
inline void handleWikiSearch();
inline void handleBooksList();
inline void handleWeatherCurrent();
inline void handleWeatherList();
inline void handleWeatherLogNow();
inline void handleWeatherInterval();
inline void handleIrSend();
inline void handleIrLearn();
inline void handleIrGetPresets();
inline void handleIrSetPresets();
inline void handleIrListSignals();
inline void handleIrSaveSignal();
inline void handleIrRenameSignal();
inline void handleIrDeleteSignal();
inline void handleRecordStart();
inline void handleRecordStop();
inline void handleRecordStatus();
inline void handleRecordingsList();
inline void handleSysConfigGet();
inline void handleSysConfigPost();
inline void handleOtaDone();
inline void handleOtaUpload();
inline void handleServerModeEnable();
inline void handleServerModeDisable();
inline void handleServerModeStatus();
inline void handleEmergencyEnable();
inline void handleEmergencyDisable();
inline void handleEmergencyStatus();
inline void handleEmergencyGetMessages();
inline void handleEmergencyPostMessage();
inline void handleEmergencyMap();
inline void handleEspNowStatus();
inline void handleVersionsList();
inline void handleVersionsRestore();
inline void handleAiBenchmark();
inline void handleFactoryReset();
inline void handleTouchDiagnostic();
inline void handleWiringDiagnostic();
inline void handleNotFound();

// ---------------------------------------------------------------------------
// 靜態資源與 PWA 路由實作
// ---------------------------------------------------------------------------
inline void handleManifestJson() {
  const char *json = "{\"name\":\"ESP32-S3 Industrial HUB\",\"short_name\":\"SD-Hub\",\"start_url\":\"/\",\"display\":\"standalone\",\"theme_color\":\"#0a0e17\",\"background_color\":\"#0a0e17\"}";
  server.send(200, "application/json; charset=utf-8", json);
}

inline void handleServiceWorkerJs() {
  const char *cleanSw = "const C='esp32-v4';self.addEventListener('install',e=>{self.skipWaiting();e.waitUntil(caches.open(C).then(c=>c.addAll(['/','/manifest.json'])));});self.addEventListener('activate',e=>e.waitUntil(clients.claim().then(()=>caches.keys().then(k=>Promise.all(k.filter(x=>x!==C).map(x=>caches.delete(x)))))));self.addEventListener('fetch',e=>{if(e.request.url.includes('/api/')||e.request.url.includes('/upload')||e.request.url.includes('/download'))return;e.respondWith(caches.match(e.request).then(r=>{const f=fetch(e.request).then(res=>{if(res&&res.status===200){const cl=res.clone();caches.open(C).then(c=>c.put(e.request,cl));}return res;}).catch(()=>r);return r||f;}));});";
  server.sendHeader("Cache-Control", "public, max-age=86400");
  server.send(200, "application/javascript; charset=utf-8", cleanSw);
}

inline void handleFavicon() {
  server.send(204, "text/plain", "");
}

inline void handleIndex() {
  uint32_t nowSec = millis() / 1000;
  if (hasFlag(SysFlag::SERVER_MODE) && serverModeEndTime > 0 && nowSec >= serverModeEndTime) {
    clearFlag(SysFlag::SERVER_MODE);
    serverModeEndTime = 0;
  }

  // 1. 若啟用了 Server Mode (自訂網站託管模式)，優先自指定目錄提供首頁
  if (hasFlag(SysFlag::SERVER_MODE) && SD.cardType() != CARD_NONE) {
    SpiLock lock;
    String customIndex = normalizePath(joinPath(serverModeRoot, "index.html"));
    if (!SD.exists(customIndex)) customIndex = normalizePath(joinPath(serverModeRoot, "index.htm"));
    if (SD.exists(customIndex)) {
      File f = SD.open(customIndex, FILE_READ);
      if (f) {
        streamFileFast(f, "text/html; charset=utf-8");
        f.close();
        return;
      }
    }
  }

  if (server.hasHeader("If-None-Match") && server.header("If-None-Match") == "\"v4.1.0\"") {
    server.send(304, "text/html", "");
    return;
  }
  if (SD.cardType() != CARD_NONE) {
    SpiLock lock;
    if (SD.exists("/index.html")) {
      auto f = SD.open("/index.html", FILE_READ);
      if (f) {
        server.streamFile(f, "text/html; charset=utf-8");
        f.close();
        return;
      }
    }
  }
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", "public, max-age=86400, stale-while-revalidate=604800");
  server.sendHeader("ETag", "\"v4.1.0\"");
  server.sendHeader("Connection", "keep-alive");
  server.send_P(200, "text/html; charset=utf-8", reinterpret_cast<const char *>(INDEX_HTML_GZ), INDEX_HTML_GZ_LEN);
}

// ---------------------------------------------------------------------------
// 使用者登入 / 登出 API
// ---------------------------------------------------------------------------
inline void handleLogin() {
  String user = server.arg("username");
  String pass = server.arg("password");

  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    String jsonUser = ConfigHelper::extractJsonVal(body, "username");
    String jsonPass = ConfigHelper::extractJsonVal(body, "password");
    if (jsonUser.length() > 0) user = jsonUser;
    if (jsonPass.length() > 0) pass = jsonPass;
  }

  user.trim();
  pass.trim();

  bool valid = false;
  if (runtimeWebPassword.length() == 0) {
    valid = true;
  } else {
    // 嚴格比對目前運行密碼與帳號，徹底移除任何後門或 hardcoded bypass
    bool passMatches = (pass == runtimeWebPassword);
    bool userMatches = (user.length() == 0 || user == runtimeWebUser);
    if (passMatches && userMatches) {
      valid = true;
    }
  }

  if (valid) {
    recordAccessLog(200);
    String token = createNewSession();
    server.sendHeader("Set-Cookie", "ESPSESSIONID=" + token + "; Path=/; Max-Age=604800; HttpOnly; SameSite=Lax");
    String resp = "{\"status\":\"ok\",\"success\":true,\"token\":\"" + token + "\",\"message\":\"登入成功\"}";
    server.send(200, "application/json; charset=utf-8", resp);
    logLine("🔑 使用者登入成功 (已發放 128-bit 隨機 Session Token)");
  } else {
    recordAccessLog(401);
    server.send(401, "application/json; charset=utf-8", "{\"status\":\"error\",\"success\":false,\"message\":\"帳號或密碼錯誤\"}");
    logLine("⚠️ 使用者登入失敗 (驗證未通過)");
  }
}

inline void handleLogout() {
  if (server.hasHeader("Cookie")) {
    String token = extractSessionTokenFromCookie(server.header("Cookie"));
    if (token.length() > 0) invalidateSession(token);
  }
  if (server.hasHeader("X-Auth-Token")) {
    String token = server.header("X-Auth-Token");
    if (token.length() > 0) invalidateSession(token);
  }
  server.sendHeader("Set-Cookie", "ESPSESSIONID=; Path=/; Max-Age=0; SameSite=Lax");
  server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"success\":true,\"message\":\"已登出\"}");
  logLine("🚪 使用者已登出 (Session 已撤銷)");
}

inline void handleAuthStatus() {
  bool auth = isClientAuthenticated();
  String json = "{\"authenticated\":" + String(auth ? "true" : "false") + ",\"user\":\"" + jsonEscape(runtimeWebUser) + "\"}";
  server.send(200, "application/json; charset=utf-8", json);
}

// ---------------------------------------------------------------------------
// 系統狀態與診斷 API
// ---------------------------------------------------------------------------
inline void handleStatus() {
  if (!requireAuth()) return;
  float temp = 0.0f;
#ifdef CONFIG_IDF_TARGET_ESP32S3
  temp = temperatureRead();
#endif
  uint32_t freeBytes = ESP.getFreeHeap();
  uint32_t freeKB    = freeBytes / 1024;
  uint32_t freqMHz   = ESP.getCpuFreqMHz();
  bool sdOk          = (SD.cardType() != CARD_NONE);

  uint64_t totalBytes = 0;
  uint64_t usedBytes  = 0;
  if (sdOk) {
    SpiLock lock;
    totalBytes = SD.totalBytes();
    usedBytes  = SD.usedBytes();
  }
  uint32_t sdTotalMB = static_cast<uint32_t>(totalBytes / (1024 * 1024));
  uint32_t sdUsedMB  = static_cast<uint32_t>(usedBytes / (1024 * 1024));

  uint32_t nowSec = millis() / 1000;
  uint32_t remSec = (serverModeEndTime > nowSec) ? (serverModeEndTime - nowSec) : 0;
  bool hasTimer   = (serverModeEndTime > 0);
  bool isEm       = hasFlag(SysFlag::EMERGENCY_MODE);

  bool hasOsm = false;
  if (sdOk) {
    SpiLock lock;
    hasOsm = (SD.exists("/SD_Card_Emergency_Tiles") || SD.exists("/emergency/tiles") || SD.exists("/emergency/map.png"));
  }

  String json;
  json.reserve(650);
  json = "{\"hasSd\":" + String(sdOk ? "true" : "false");
  json += ",\"sdCardOk\":" + String(sdOk ? "true" : "false");
  json += ",\"sdTotalMB\":" + String(sdTotalMB > 0 ? sdTotalMB : 29800);
  json += ",\"sdUsedMB\":" + String(sdUsedMB);
  json += ",\"hasHtmlFolder\":" + String(SD.exists("/html") ? "true" : "false");
  json += ",\"isServerMode\":" + String(hasFlag(SysFlag::SERVER_MODE) ? "true" : "false");
  json += ",\"serverRoot\":\"" + jsonEscape(serverModeRoot) + "\"";
  json += ",\"root\":\"" + jsonEscape(serverModeRoot) + "\"";
  json += ",\"hasTimer\":" + String(hasTimer ? "true" : "false");
  json += ",\"remSec\":" + String(remSec);
  json += ",\"isEmergencyMode\":" + String(isEm ? "true" : "false");
  json += ",\"hasOsmTiles\":" + String(hasOsm ? "true" : "false");
  json += ",\"freeHeap\":" + String(freeBytes);
  json += ",\"freeHeapKB\":" + String(freeKB);
  json += ",\"freePsram\":" + String(ESP.getFreePsram());
  json += ",\"cpuFreq\":" + String(freqMHz);
  json += ",\"cpuFreqMHz\":" + String(freqMHz);
  json += ",\"temp\":" + String(temp, 1);
  json += ",\"usbMounted\":" + String(hasFlag(SysFlag::USB_MOUNTED) ? "true" : "false");
  json += ",\"throttled\":" + String(hasFlag(SysFlag::THERMAL_THROTTLED) ? "true" : "false");
  json += ",\"apSsid\":\"" + jsonEscape(runtimeApSsid) + "\"";
  json += ",\"apPass\":\"" + jsonEscape(runtimeApPassword) + "\"";
  json += "}";
  server.send(200, "application/json; charset=utf-8", json);
}

inline void handleDiagnose() { handleStatus(); }

inline void handleLogs() {
  if (!requireAuth()) return;
  String json;
  json.reserve(1024);
  json = "{\"status\":\"ok\",\"logs\":[";
  for (size_t i = 0; i < accessLogCount; ++i) {
    size_t idx = (accessLogHead + MAX_ACCESS_LOGS - accessLogCount + i) % MAX_ACCESS_LOGS;
    if (i > 0) json += ",";
    uint32_t s = accessLogs[idx].uptime;
    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u:%02u", (s / 3600) % 24, (s / 60) % 60, s % 60);
    json += "{\"time\":\"" + String(timeBuf) + "\",\"ip\":\"" + jsonEscape(String(accessLogs[idx].ip)) + "\",\"method\":\"" + jsonEscape(String(accessLogs[idx].method)) + "\",\"path\":\"" + jsonEscape(String(accessLogs[idx].path)) + "\",\"status\":" + String(accessLogs[idx].status) + "}";
  }
  json += "]}";
  server.send(200, "application/json; charset=utf-8", json);
}

inline void handleLogsClear() {
  if (!requireAuth()) return;
  accessLogHead = 0;
  accessLogCount = 0;
  server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"message\":\"日誌已清除\"}");
}

// ---------------------------------------------------------------------------
// 檔案系統瀏覽與操作 API (全面 SpiLock 保護)
// ---------------------------------------------------------------------------
inline void handleList() {
  if (!requireAuth()) return;
  server.sendHeader("Connection", "keep-alive");

  String rawPath = server.hasArg("path") ? server.arg("path") : (server.hasArg("dir") ? server.arg("dir") : "/");
  rawPath.trim();
  if (rawPath == "%2F" || rawPath == "%2f" || rawPath.length() == 0) {
    rawPath = "/";
  }

  String path = normalizePath(rawPath);
  if (path.length() == 0) path = "/";

  if (SD.cardType() == CARD_NONE) {
    server.send(200, "application/json; charset=utf-8", "{\"path\":\"/\",\"entries\":[],\"warning\":\"SD卡未掛載\"}");
    return;
  }

  SpiLock lock;
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) {
    if (path == "/") {
      server.send(200, "application/json; charset=utf-8", "{\"path\":\"/\",\"entries\":[]}");
      return;
    }
    server.send(404, "application/json; charset=utf-8", "{\"error\":\"not_found\",\"message\":\"資料夾不存在\"}");
    return;
  }

  String json;
  json.reserve(1024);
  json = "{\"path\":\"" + jsonEscape(path) + "\",\"entries\":[";
  bool first = true;
  File entry = dir.openNextFile();
  while (entry) {
    if (!first) json += ",";
    first = false;
    json += "{\"name\":\"" + jsonEscape(leafName(String(entry.name()))) + "\",\"isDir\":" + (entry.isDirectory() ? "true" : "false") + ",\"size\":" + String(entry.size()) + "}";
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
  json += "]}";
  server.send(200, "application/json; charset=utf-8", json);
}

inline void handleDownload() {
  if (!requireAuth()) return;
  auto path = normalizePath(server.arg("path"));
  SpiLock lock;
  auto file = SD.open(path, FILE_READ);
  if (!file || file.isDirectory()) return sendText(404, "檔案不存在");
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + leafName(path) + "\"");
  streamFileFast(file, getMIMEType(path));
  file.close();
}

inline void handleView() {
  if (!requireAuth()) return;
  auto path = normalizePath(server.arg("path"));
  SpiLock lock;
  auto file = SD.open(path, FILE_READ);
  if (!file || file.isDirectory()) return sendText(404, "檔案不存在");
  streamFileFast(file, getMIMEType(path));
  file.close();
}

inline void handleMkdir() {
  if (!requireAuth()) return;
  String path = server.arg("path");
  if (path.length() == 0) {
    String parent = server.arg("parent");
    String name   = server.arg("name");
    path = joinPath(parent, name);
  }
  path = normalizePath(path);
  if (path.length() <= 1) return sendText(400, "無效的資料夾路徑");

  if (ensureDirectoryExists(path)) sendText(200, "OK");
  else sendText(500, "建立資料夾失敗");
}

inline void handleRename() {
  if (!requireAuth()) return;
  String oldPath = server.arg("oldPath");
  if (oldPath.length() == 0) oldPath = server.arg("path");
  if (oldPath.length() == 0) oldPath = server.arg("from");
  oldPath = normalizePath(oldPath);

  String newPath = server.arg("newPath");
  if (newPath.length() == 0) {
    String newName = server.arg("name");
    if (newName.length() > 0) newPath = joinPath(parentPath(oldPath), newName);
  }
  if (newPath.length() == 0) {
    String toDir = server.arg("toDir");
    if (toDir.length() > 0) newPath = joinPath(toDir, leafName(oldPath));
  }
  newPath = normalizePath(newPath);

  if (oldPath.length() <= 1 || newPath.length() <= 1 || oldPath == newPath) {
    return sendText(400, "無效的來源或目標路徑");
  }

  ensureDirectoryExists(parentPath(newPath));

  SpiLock lock;
  if (SD.rename(oldPath, newPath)) {
    sendText(200, "OK");
  } else {
    sendText(500, "重新命名或移動失敗");
  }
}

inline void handleMove() { handleRename(); }

inline void handleDelete() {
  if (!requireAuth()) return;
  auto path = normalizePath(server.arg("path"));
  if (path.length() == 0 || path == "/" || path == "/.system") {
    server.send(400, "application/json; charset=utf-8", "{\"status\":\"error\",\"error\":\"無法刪除根目錄\"}");
    return;
  }
  if (removeRecursive(path)) {
    server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"success\":true,\"message\":\"刪除成功\"}");
  } else {
    server.send(500, "application/json; charset=utf-8", "{\"status\":\"error\",\"success\":false,\"error\":\"刪除失敗\"}");
  }
}

inline void handleUploadData() {
  if (server.uri() != "/upload") return;
  if (!isClientAuthenticated()) {
    // 未授權請求立即中止上傳並標記錯誤
    uploadError = "未授權的上傳請求";
    return;
  }

  SpiLock lock;
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    g_isUploadingActive = true;
    uploadError = "";
    uploadReceivedBytes = 0;

    String dir = server.hasArg("dir") ? server.arg("dir") : (server.hasArg("path") ? server.arg("path") : "/");
    dir = normalizePath(dir);
    if (dir.length() == 0) dir = "/";

    String fname = upload.filename;
    if (server.hasArg("relPath") && server.arg("relPath").length() > 0) {
      fname = server.arg("relPath");
    }
    // 嚴格正規化並防禦路徑穿越
    uploadTarget = normalizePath(joinPath(dir, fname));

    // 禁止未授權直接覆蓋關鍵系統設定
    if (uploadTarget == "/config.json" && !server.hasArg("allow_overwrite_config")) {
      uploadError = "禁止直接覆蓋系統核心設定檔 (/config.json)";
      g_isUploadingActive = false;
      return;
    }

    ensureDirectoryExists(parentPath(uploadTarget));
    if (SD.exists(uploadTarget)) {
      createFileVersionBackup(uploadTarget);
      SD.remove(uploadTarget);
    }
    uploadFile = SD.open(uploadTarget, FILE_WRITE);
    if (uploadFile) {
      logf("📤 [Upload] 開始接收檔案: %s\n", uploadTarget.c_str());
    } else {
      uploadError = "無法在 SD 卡建立檔案: " + uploadTarget;
      g_isUploadingActive = false;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile && upload.currentSize > 0 && uploadError.length() == 0) {
      uploadFile.write(upload.buf, upload.currentSize);
      uploadReceivedBytes += upload.currentSize;
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.flush();
      uploadFile.close();
      logf("✅ [Upload] 檔案接收完成: %s (%u Bytes)\n", uploadTarget.c_str(), upload.totalSize);
    }
    g_isUploadingActive = false;
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) {
      uploadFile.close();
      SD.remove(uploadTarget);
    }
    g_isUploadingActive = false;
    uploadError = "使用者取消上傳或傳輸中斷";
  }
}

inline void handleUploadDone() {
  g_isUploadingActive = false;
  if (!requireAuth()) return;

  server.sendHeader("Access-Control-Allow-Origin", "*");
  if (uploadError.length() > 0) {
    server.send(400, "application/json; charset=utf-8", "{\"status\":\"error\",\"success\":false,\"message\":\"" + jsonEscape(uploadError) + "\"}");
  } else {
    server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"success\":true,\"message\":\"上傳成功\",\"path\":\"" + jsonEscape(uploadTarget) + "\"}");
  }
}

inline void createFileVersionBackup(const String &path) {
  SpiLock lock;
  if (!SD.exists(path)) return;
  ensureDirectoryExists("/.versions");
  auto fname = leafName(path);
  auto ts = millis() / 1000;
  auto backupPath = "/.versions/" + fname + "_" + String(ts) + ".bak";

  auto src = SD.open(path, FILE_READ);
  auto dst = SD.open(backupPath, FILE_WRITE);
  if (src && dst) {
    uint8_t buf[512];
    while (src.available()) {
      auto len = src.read(buf, sizeof(buf));
      dst.write(buf, len);
    }
  }
  if (src) src.close();
  if (dst) { dst.flush(); dst.close(); }
}

inline void handleRead() {
  if (!requireAuth()) return;
  auto path = normalizePath(server.arg("path"));
  SpiLock lock;
  auto file = SD.open(path, FILE_READ);
  if (!file) return sendText(404, "檔案不存在");
  streamFileFast(file, "text/plain; charset=utf-8");
  file.close();
}

inline void handleWrite() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");

  if (server.method() == HTTP_OPTIONS) {
    server.send(204);
    return;
  }

  if (!requireAuth()) return;

  if (SD.cardType() == CARD_NONE) {
    sendText(500, "SD卡未掛載");
    return;
  }

  String path = "";
  String content = "";

  if (server.hasArg("path") && server.arg("path").length() > 0) {
    path = server.arg("path");
  } else if (server.hasArg("file") && server.arg("file").length() > 0) {
    path = server.arg("file");
  }
  if (server.hasArg("content")) {
    content = server.arg("content");
  }

  if ((path.length() == 0 || content.length() == 0) && server.hasArg("plain")) {
    String plain = server.arg("plain");
    if (plain.startsWith("{") && plain.indexOf("\"path\"") != -1) {
      path = ConfigHelper::extractJsonVal(plain, "path");
      content = ConfigHelper::extractJsonVal(plain, "content");
    } else if (plain.indexOf("path=") != -1 || plain.indexOf("content=") != -1) {
      int pIdx = plain.indexOf("path=");
      int cIdx = plain.indexOf("content=");
      if (pIdx != -1) {
        int pEnd = (cIdx > pIdx) ? cIdx : plain.length();
        if (pEnd > pIdx && plain[pEnd - 1] == '&') pEnd--;
        String rawPath = plain.substring(pIdx + 5, pEnd);
        if (path.length() == 0) path = urlDecode(rawPath);
      }
      if (cIdx != -1) {
        String rawContent = plain.substring(cIdx + 8);
        content = urlDecode(rawContent);
      }
    }
  }

  path = normalizePath(path);
  if (path.length() <= 1) {
    sendText(400, "無效的檔案路徑");
    return;
  }

  createFileVersionBackup(path);
  ensureDirectoryExists(parentPath(path));

  SpiLock lock;
  if (SD.exists(path)) {
    SD.remove(path);
  }

  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    sendText(500, "無法建立或開啟檔案：" + path);
    return;
  }

  if (content.length() > 0) {
    file.print(content);
  }
  file.flush();
  file.close();

  logf("💾 [Write] 成功寫入檔案: %s (%u Bytes)\n", path.c_str(), (unsigned)content.length());
  server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"success\":true,\"message\":\"儲存成功\"}");
}

inline void handleUnzip() {
  if (!requireAuth()) return;
  auto zipPath = normalizePath(server.arg("path"));
  auto destDir = normalizePath(server.arg("destDir"));
  if (destDir.length() == 0) destDir = parentPath(zipPath);
  int extracted = 0, skipped = 0;
  if (unzipFile(zipPath, destDir, extracted, skipped)) {
    server.send(200, "application/json; charset=utf-8", "{\"extracted\":" + String(extracted) + ",\"skipped\":" + String(skipped) + "}");
  } else {
    sendText(500, "解壓縮失敗，請確認檔案為無密碼標準 ZIP 格式");
  }
}

// ---------------------------------------------------------------------------
// Wi-Fi 與 NAPT 中繼 API
// ---------------------------------------------------------------------------
inline void handleWiFiScan() {
  int16_t n = WiFi.scanNetworks(false, true, false, 120);
  if (n < 0) {
    n = WiFi.scanNetworks(false, false, false, 150);
  }
  String json;
  json.reserve(1024);
  json = "[";
  if (n > 0) {
    for (int i = 0; i < n; ++i) {
      if (WiFi.SSID(i).length() == 0) continue;
      if (json.length() > 1) json += ",";
      json += "{\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + ",\"channel\":" + String(WiFi.channel(i)) + ",\"encrypted\":" + (WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
    }
  }
  json += "]";
  WiFi.scanDelete();
  server.send(200, "application/json; charset=utf-8", json);
}

inline void handleWiFiConnect() {
  if (!requireAuth()) return;
  String ssid     = server.arg("ssid");
  String password = server.arg("password");
  if (ssid.length() == 0) return sendText(400, "SSID 不能為空");
  logf("📶 [Wi-Fi 連線] 正在連線至：%s\n", ssid.c_str());
  WiFi.disconnect();
  yield();
  WiFi.begin(ssid.c_str(), password.c_str());
  sendText(200, "OK");
}

inline void handleWiFiDisconnect() {
  if (!requireAuth()) return;
  WiFi.disconnect();
  sendText(200, "已中斷連線");
}

inline void handleWiFiStatus() {
  String json;
  json.reserve(200);
  bool isConn = (WiFi.status() == WL_CONNECTED);
  json = "{\"staConnected\":" + String(isConn ? "true" : "false");
  json += ",\"staSSID\":\"" + jsonEscape(WiFi.SSID()) + "\"";
  json += ",\"staIP\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"repeater\":" + String(hasFlag(SysFlag::NAPT_ENABLED) ? "true" : "false");
  json += "}";
  server.send(200, "application/json; charset=utf-8", json);
}

inline void handleRepeaterEnable() {
  if (!requireAuth()) return;
  enableNAPTRepeater();
  sendText(200, "NAPT 中繼已開啟");
}

inline void handleRepeaterDisable() {
  if (!requireAuth()) return;
  disableNAPTRepeater();
  sendText(200, "NAPT 中繼已關閉");
}

inline void handleRepeaterPost() {
  if (!requireAuth()) return;
  String enableStr = server.arg("enable");
  if (enableStr == "true" || enableStr == "1") {
    enableNAPTRepeater();
    server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"repeater\":true}");
  } else {
    disableNAPTRepeater();
    server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"repeater\":false}");
  }
}

// ---------------------------------------------------------------------------
// 萬用紅外線遙控 API
// ---------------------------------------------------------------------------
inline void handleIrSend() {
  if (!requireAuth()) return;

  if (server.hasArg("hex") && server.arg("hex").length() >= 16) {
    String hexStr = server.arg("hex");
    hexStr.trim();
    uint8_t bytes[64];
    uint8_t len = 0;
    for (size_t i = 0; i + 1 < hexStr.length() && len < 64; i += 2) {
      char byteBuf[3] = { hexStr[i], hexStr[i + 1], 0 };
      bytes[len++] = static_cast<uint8_t>(strtoul(byteBuf, nullptr, 16));
    }
    if (len >= 27) {
      sendPanasonicAcBytes(bytes, len);
      server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"mode\":\"panasonic_ac\"}");
      return;
    }
  }

  uint32_t carrierFreq = 38000;
  if (server.hasArg("freq")) {
    uint32_t f = server.arg("freq").toInt();
    if (f >= 10000 && f <= 100000) carrierFreq = f;
  }

  if (server.hasArg("raw") && server.arg("raw").length() > 5) {
    String rawStr = server.arg("raw");
    sendRawIrString(rawStr, carrierFreq);
    server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"mode\":\"raw\",\"freq\":" + String(carrierFreq) + "}");
    return;
  }

  String codeStr = server.arg("code");
  if (codeStr.length() == 0) {
    server.send(400, "application/json; charset=utf-8", "{\"status\":\"error\",\"message\":\"缺少 code 參數\"}");
    return;
  }
  uint32_t code = strtoul(codeStr.c_str(), nullptr, 16);
  String protocol = server.hasArg("protocol") ? server.arg("protocol") : "NEC";
  protocol.toUpperCase();

  if (protocol == "SONY") {
    uint8_t bits = server.hasArg("bits") ? server.arg("bits").toInt() : 12;
    if (bits == 0) bits = 12;
    sendSonyIrCode(code, bits);
    String resp = "{\"status\":\"ok\",\"sentCode\":\"" + codeStr + "\",\"protocol\":\"SONY\",\"freq\":40000,\"bits\":" + String(bits) + "}";
    server.send(200, "application/json; charset=utf-8", resp);
  } else {
    sendNecIrCode(code);
    String resp = "{\"status\":\"ok\",\"sentCode\":\"" + codeStr + "\",\"protocol\":\"NEC\",\"freq\":38000}";
    server.send(200, "application/json; charset=utf-8", resp);
  }
}

inline void handleIrLearn() {
  if (!requireAuth()) return;
  if (server.hasArg("reset")) {
    lastLearnedIr.hasData = false;
    lastLearnedIr.code = 0;
    lastLearnedIr.rawLen = 0;
    lastLearnedIr.analysis = "";
  }

  uint32_t wStart = millis();
  while (millis() - wStart < 600) {
    if (checkIrReceiver()) break;
    yield();
  }

  String rawStr = "";
  if (lastLearnedIr.hasData && lastLearnedIr.rawLen > 0) {
    for (uint16_t i = 0; i < lastLearnedIr.rawLen; ++i) {
      rawStr += String(lastLearnedIr.rawPulses[i]);
      if (i + 1 < lastLearnedIr.rawLen) rawStr += ",";
    }
  }

  char codeHex[16];
  snprintf(codeHex, sizeof(codeHex), "0x%08X", (unsigned)lastLearnedIr.code);

  String resp = String("{\"status\":\"ok\"")
              + ",\"hasData\":" + (lastLearnedIr.hasData ? "true" : "false")
              + ",\"code\":\"" + String(codeHex) + "\""
              + ",\"hex\":\"" + lastLearnedIr.hexStr + "\""
              + ",\"protocol\":\"" + lastLearnedIr.protocol + "\""
              + ",\"bits\":" + String(lastLearnedIr.bits)
              + ",\"raw\":\"" + rawStr + "\""
              + ",\"analysis\":\"" + jsonEscape(lastLearnedIr.analysis) + "\"}";

  server.send(200, "application/json; charset=utf-8", resp);
}

inline void handleIrGetPresets() {
  if (!requireAuth()) return;
  loadIrPresetsFromSD();
  String out = "[";
  for (int i = 0; i < 6; ++i) {
    char codeHex[16], code2Hex[16];
    snprintf(codeHex, sizeof(codeHex), "0x%08X", (unsigned)g_irPresets[i].code);
    snprintf(code2Hex, sizeof(code2Hex), "0x%08X", (unsigned)g_irPresets[i].code2);
    out += "{\"slot\":" + String(i) + ",\"label\":\"" + jsonEscape(String(g_irPresets[i].label)) + "\",\"code\":\"" + String(codeHex) + "\",\"code2\":\"" + String(code2Hex) + "\",\"delayMs\":" + String(g_irPresets[i].delayMs > 0 ? g_irPresets[i].delayMs : 700) + "}";
    if (i < 5) out += ",";
  }
  out += "]";
  server.send(200, "application/json; charset=utf-8", out);
}

inline void handleIrSetPresets() {
  if (!requireAuth()) return;
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    for (int i = 0; i < 6; ++i) {
      String slotKey = "\"slot\":" + String(i);
      int slotPos = body.indexOf(slotKey);
      if (slotPos == -1) slotPos = body.indexOf("\"id\":" + String(i));
      if (slotPos != -1) {
        int labelPos = body.indexOf("\"label\":\"", slotPos);
        if (labelPos != -1 && labelPos < slotPos + 200) {
          int lStart = labelPos + 9;
          int lEnd = body.indexOf('"', lStart);
          if (lEnd != -1) {
            String lbl = body.substring(lStart, lEnd);
            lbl.trim();
            if (lbl.length() > 0) {
              strncpy(g_irPresets[i].label, lbl.c_str(), sizeof(g_irPresets[i].label) - 1);
              g_irPresets[i].label[sizeof(g_irPresets[i].label) - 1] = '\0';
            }
          }
        }
        int codePos = body.indexOf("\"code\":\"", slotPos);
        if (codePos != -1 && codePos < slotPos + 200) {
          int cStart = codePos + 8;
          int cEnd = body.indexOf('"', cStart);
          if (cEnd != -1) {
            String cStr = body.substring(cStart, cEnd);
            cStr.trim();
            if (cStr.length() > 0) g_irPresets[i].code = strtoul(cStr.c_str(), nullptr, 16);
          }
        }
        int code2Pos = body.indexOf("\"code2\":\"", slotPos);
        if (code2Pos != -1 && code2Pos < slotPos + 300) {
          int c2Start = code2Pos + 9;
          int c2End = body.indexOf('"', c2Start);
          if (c2End != -1) {
            String c2Str = body.substring(c2Start, c2End);
            c2Str.trim();
            g_irPresets[i].code2 = (c2Str.length() > 0 && c2Str != "0x00000000" && c2Str != "0") ? strtoul(c2Str.c_str(), nullptr, 16) : 0;
          }
        }
        int delayPos = body.indexOf("\"delayMs\":", slotPos);
        if (delayPos != -1 && delayPos < slotPos + 350) {
          int dStart = delayPos + 10;
          int dEnd1 = body.indexOf(',', dStart);
          int dEnd2 = body.indexOf('}', dStart);
          int dEnd = (dEnd1 != -1 && (dEnd2 == -1 || dEnd1 < dEnd2)) ? dEnd1 : dEnd2;
          if (dEnd != -1) {
            uint16_t d = body.substring(dStart, dEnd).toInt();
            g_irPresets[i].delayMs = (d > 0) ? d : 700;
          }
        }
      }
    }
  }

  saveIrPresetsToSD();
  if (currentPage == PAGE_IR_REMOTE) {
    renderScreenIrRemote();
  }
  server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\"}");
}

inline void handleIrListSignals() {
  if (!requireAuth()) return;
  ensureDirectoryExists("/config");
  String path = "/config/ir_signals.json";
  SpiLock lock;
  if (SD.exists(path)) {
    File f = SD.open(path, FILE_READ);
    if (f) {
      server.streamFile(f, "application/json; charset=utf-8");
      f.close();
      return;
    }
  }
  server.send(200, "application/json; charset=utf-8", "[]");
}

inline void handleIrSaveSignal() {
  if (!requireAuth()) return;
  String name      = server.arg("name");
  String type      = server.hasArg("type") ? server.arg("type") : "single";
  String code      = server.arg("code");
  String codeOn    = server.arg("codeOn");
  String codeOff   = server.arg("codeOff");
  String codeUp    = server.arg("codeUp");
  String codeDown  = server.arg("codeDown");
  String rawOn     = server.arg("rawOn");
  String rawOff    = server.arg("rawOff");
  String rawUp     = server.arg("rawUp");
  String rawDown   = server.arg("rawDown");
  String rawSingle = server.arg("raw");
  String analysis  = server.arg("analysis");
  String protocol  = server.arg("protocol");
  if (protocol.length() == 0) protocol = "NEC";

  if (name.length() == 0) {
    server.send(400, "application/json; charset=utf-8", "{\"status\":\"error\",\"message\":\"缺少設備名稱\"}");
    return;
  }

  ensureDirectoryExists("/config");
  String path = "/config/ir_signals.json";
  String currentJson = "[]";
  {
    SpiLock lock;
    if (SD.exists(path)) {
      File f = SD.open(path, FILE_READ);
      if (f) {
        currentJson = f.readString();
        f.close();
      }
    }
  }
  currentJson.trim();
  if (!currentJson.startsWith("[")) currentJson = "[]";

  String id = "ir_" + String(static_cast<uint32_t>(millis() / 1000)) + "_" + String(random(1000, 9999));
  String primaryCode = (code.length() > 0) ? code : ((codeOn.length() > 0) ? codeOn : codeUp);

  String newEntry = String("{\"id\":\"") + jsonEscape(id) + "\""
                  + ",\"name\":\"" + jsonEscape(name) + "\""
                  + ",\"type\":\"" + jsonEscape(type) + "\""
                  + ",\"code\":\"" + jsonEscape(primaryCode) + "\""
                  + ",\"codeOn\":\"" + jsonEscape(codeOn) + "\""
                  + ",\"codeOff\":\"" + jsonEscape(codeOff) + "\""
                  + ",\"codeUp\":\"" + jsonEscape(codeUp) + "\""
                  + ",\"codeDown\":\"" + jsonEscape(codeDown) + "\""
                  + ",\"rawOn\":\"" + jsonEscape(rawOn) + "\""
                  + ",\"rawOff\":\"" + jsonEscape(rawOff) + "\""
                  + ",\"rawUp\":\"" + jsonEscape(rawUp) + "\""
                  + ",\"rawDown\":\"" + jsonEscape(rawDown) + "\""
                  + ",\"raw\":\"" + jsonEscape(rawSingle) + "\""
                  + ",\"analysis\":\"" + jsonEscape(analysis) + "\""
                  + ",\"protocol\":\"" + jsonEscape(protocol) + "\"}";

  String updatedJson;
  if (currentJson == "[]" || currentJson.length() <= 2) {
    updatedJson = "[" + newEntry + "]";
  } else {
    int lastBracket = currentJson.lastIndexOf(']');
    if (lastBracket != -1) {
      updatedJson = currentJson.substring(0, lastBracket) + "," + newEntry + "]";
    } else {
      updatedJson = "[" + newEntry + "]";
    }
  }

  SpiLock lock;
  File f = SD.open(path, FILE_WRITE);
  if (f) {
    f.print(updatedJson);
    f.flush();
    f.close();
    server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"id\":\"" + id + "\"}");
  } else {
    server.send(500, "application/json; charset=utf-8", "{\"status\":\"error\",\"message\":\"寫入 SD 檔案失敗\"}");
  }
}

inline void handleIrRenameSignal() {
  if (!requireAuth()) return;
  String id      = server.arg("id");
  String newName = server.arg("name");
  if (id.length() == 0 || newName.length() == 0) {
    server.send(400, "application/json; charset=utf-8", "{\"status\":\"error\",\"message\":\"缺少 id 或 name\"}");
    return;
  }

  String path = "/config/ir_signals.json";
  SpiLock lock;
  if (!SD.exists(path)) {
    server.send(404, "application/json; charset=utf-8", "{\"status\":\"error\",\"message\":\"檔案不存在\"}");
    return;
  }

  File f = SD.open(path, FILE_READ);
  String json = f.readString();
  f.close();

  int idPos = json.indexOf("\"id\":\"" + id + "\"");
  if (idPos != -1) {
    int nameKeyPos = json.lastIndexOf("\"name\":\"", idPos);
    if (nameKeyPos == -1) nameKeyPos = json.indexOf("\"name\":\"", idPos);
    if (nameKeyPos != -1) {
      int valStart = nameKeyPos + 8;
      int valEnd   = json.indexOf('"', valStart);
      if (valEnd != -1) {
        json = json.substring(0, valStart) + jsonEscape(newName) + json.substring(valEnd);
        File fw = SD.open(path, FILE_WRITE);
        if (fw) {
          fw.print(json);
          fw.flush();
          fw.close();
          server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\"}");
          return;
        }
      }
    }
  }
  server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\"}");
}

inline void handleIrDeleteSignal() {
  if (!requireAuth()) return;
  String id = server.arg("id");
  if (id.length() == 0) {
    server.send(400, "application/json; charset=utf-8", "{\"status\":\"error\",\"message\":\"缺少 id\"}");
    return;
  }

  String path = "/config/ir_signals.json";
  SpiLock lock;
  if (!SD.exists(path)) {
    server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\"}");
    return;
  }

  File f = SD.open(path, FILE_READ);
  String json = f.readString();
  f.close();

  int idPos = json.indexOf("\"id\":\"" + id + "\"");
  if (idPos != -1) {
    int objStart = json.lastIndexOf('{', idPos);
    int objEnd   = json.indexOf('}', idPos);
    if (objStart != -1 && objEnd != -1) {
      if (objStart > 1 && json[objStart - 1] == ',') objStart--;
      else if (objEnd < (int)json.length() - 2 && json[objEnd + 1] == ',') objEnd++;
      json = json.substring(0, objStart) + json.substring(objEnd + 1);

      File fw = SD.open(path, FILE_WRITE);
      if (fw) {
        fw.print(json);
        fw.flush();
        fw.close();
      }
    }
  }
  server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\"}");
}

// ---------------------------------------------------------------------------
// I2S 音訊錄音 API
// ---------------------------------------------------------------------------
inline void handleRecordStart() {
  if (!requireAuth()) return;
  bool ok = startAudioRecording();
  if (ok) {
    String resp = "{\"status\":\"recording\",\"filename\":\"" + jsonEscape(audioState.currentFilename) + "\"}";
    server.send(200, "application/json; charset=utf-8", resp);
  } else {
    String msg = (SD.cardType() == CARD_NONE) ? "SD 卡未就緒" : "無法建立 SD 錄音檔";
    server.send(500, "application/json; charset=utf-8", "{\"status\":\"error\",\"message\":\"" + msg + "\"}");
  }
}

inline void handleRecordStop() {
  if (!requireAuth()) return;
  String finishedFile   = audioState.currentFilename;
  uint32_t bytesWritten = audioState.totalBytesWritten;
  stopAudioRecording();
  String resp = "{\"status\":\"stopped\",\"filename\":\"" + jsonEscape(finishedFile) + "\",\"size\":" + String(bytesWritten) + "}";
  server.send(200, "application/json; charset=utf-8", resp);
}

inline void handleRecordStatus() {
  String json = "{\"isRecording\":" + String(audioState.isRecording ? "true" : "false");
  json += ",\"filename\":\"" + jsonEscape(audioState.currentFilename) + "\"";
  json += ",\"durationSec\":" + String(audioState.durationSec);
  json += ",\"bytesWritten\":" + String(audioState.totalBytesWritten);
  json += ",\"peak\":" + String(audioState.lastPeakVolume);
  json += "}";
  server.send(200, "application/json; charset=utf-8", json);
}

inline void handleRecordingsList() {
  if (!requireAuth()) return;
  String json;
  json.reserve(512);
  json = "{\"status\":\"ok\",\"files\":[";
  bool first = true;
  if (SD.cardType() != CARD_NONE) {
    SpiLock lock;
    if (SD.exists("/recordings")) {
      File dir = SD.open("/recordings");
      if (dir && dir.isDirectory()) {
        File file = dir.openNextFile();
        while (file) {
          if (!file.isDirectory()) {
            String fn = leafName(String(file.name()));
            String lower = fn;
            lower.toLowerCase();
            if (lower.endsWith(".wav")) {
              if (!first) json += ",";
              first = false;
              json += "{\"name\":\"" + jsonEscape(fn) + "\",\"size\":" + String(file.size()) + "}";
            }
          }
          file = dir.openNextFile();
        }
        dir.close();
      }
    }
  }
  json += "]}";
  server.send(200, "application/json; charset=utf-8", json);
}

// ---------------------------------------------------------------------------
// 離線生產力：Wiki / 電子書 / 氣象站
// ---------------------------------------------------------------------------
inline void handleWikiSearch() {
  SpiLock lock;
  String q = server.arg("q");
  q.toLowerCase();
  ensureDirectoryExists("/wiki");
  File dir = SD.open("/wiki");
  String json;
  json.reserve(512);
  json = "[";
  bool first = true;
  if (dir && dir.isDirectory()) {
    File entry = dir.openNextFile();
    while (entry) {
      String fname = leafName(String(entry.name()));
      String fnameLower = fname;
      fnameLower.toLowerCase();
      if (fnameLower.endsWith(".txt") || fnameLower.endsWith(".md")) {
        if (q.length() == 0 || fnameLower.indexOf(q) >= 0) {
          if (!first) json += ",";
          first = false;
          json += "{\"name\":\"" + jsonEscape(fname) + "\",\"path\":\"" + jsonEscape(joinPath("/wiki", fname)) + "\",\"size\":" + String(entry.size()) + "}";
        }
      }
      entry.close();
      entry = dir.openNextFile();
    }
    dir.close();
  }
  json += "]";
  server.send(200, "application/json; charset=utf-8", json);
}

inline void handleBooksList() {
  SpiLock lock;
  ensureDirectoryExists("/books");
  File dir = SD.open("/books");
  String json;
  json.reserve(512);
  json = "[";
  bool first = true;
  if (dir && dir.isDirectory()) {
    File entry = dir.openNextFile();
    while (entry) {
      String fname = leafName(String(entry.name()));
      String fnameLower = fname;
      fnameLower.toLowerCase();
      if (fnameLower.endsWith(".txt") || fnameLower.endsWith(".md") || fnameLower.endsWith(".epub")) {
        if (!first) json += ",";
        first = false;
        json += "{\"name\":\"" + jsonEscape(fname) + "\",\"path\":\"" + jsonEscape(joinPath("/books", fname)) + "\",\"size\":" + String(entry.size()) + "}";
      }
      entry.close();
      entry = dir.openNextFile();
    }
    dir.close();
  }
  json += "]";
  server.send(200, "application/json; charset=utf-8", json);
}

inline void handleWeatherCurrent() {
  float chipTemp = 0.0f;
#ifdef CONFIG_IDF_TARGET_ESP32S3
  chipTemp = temperatureRead();
#endif
  uint32_t nowSec = millis() / 1000;
  // 提供真實晶片結溫監控，清楚標示目前無實體外部溫濕度感測器 (避免假數值誤導)
  String json = "{\"uptime\":" + String(nowSec)
              + ",\"chip_temp\":" + String(chipTemp, 1)
              + ",\"has_external_sensor\":false"
              + ",\"cpuFreq\":" + String(ESP.getCpuFreqMHz())
              + ",\"freeHeap\":" + String(ESP.getFreeHeap())
              + ",\"freePsram\":" + String(ESP.getFreePsram())
              + ",\"interval\":" + String(weatherLogIntervalSec)
              + ",\"totalLogs\":" + String(totalWeatherLogCount) + "}";
  server.send(200, "application/json; charset=utf-8", json);
}

inline void handleWeatherList() {
  SpiLock lock;
  ensureWeatherDir();
  File dir = SD.open("/weather");
  String json;
  json.reserve(512);
  json = "[";
  bool first = true;
  if (dir && dir.isDirectory()) {
    File entry = dir.openNextFile();
    while (entry) {
      String fname = leafName(String(entry.name()));
      if (!first) json += ",";
      first = false;
      json += "{\"name\":\"" + jsonEscape(fname) + "\",\"path\":\"" + jsonEscape(joinPath("/weather", fname)) + "\",\"size\":" + String(entry.size()) + "}";
      entry.close();
      entry = dir.openNextFile();
    }
    dir.close();
  }
  json += "]";
  server.send(200, "application/json; charset=utf-8", json);
}

inline void handleWeatherLogNow() {
  if (!requireAuth()) return;
  appendWeatherRecordCsv();
  sendText(200, "OK");
}

inline void handleWeatherInterval() {
  if (!requireAuth()) return;
  int sec = server.arg("sec").toInt();
  if (sec >= 5 && sec <= 86400) {
    weatherLogIntervalSec = sec;
    sendText(200, "OK");
  } else {
    sendText(400, "無效的記錄週期");
  }
}

// ---------------------------------------------------------------------------
// 系統進階託管模式 / 急難門戶 / OTA / Benchmark
// ---------------------------------------------------------------------------
inline void handleServerModeEnable() {
  if (!requireAuth()) return;
  String root = server.arg("root");
  if (root.length() == 0) root = server.arg("path");
  if (root.length() > 0)  serverModeRoot = normalizePath(root);
  auto duration = server.arg("duration").toInt();
  if (duration > 0) serverModeEndTime = (millis() / 1000) + (duration * 60);
  else serverModeEndTime = 0;
  setFlag(SysFlag::SERVER_MODE);
  String resp = "{\"status\":\"ok\",\"success\":true,\"isServerMode\":true,\"root\":\"" + jsonEscape(serverModeRoot) + "\",\"duration\":" + String(duration) + "}";
  server.send(200, "application/json; charset=utf-8", resp);
  logf("🚀 [Server Mode] 已啟動靜態網站託管模式 (Root: %s, 剩餘時效: %d 分鐘)\n", serverModeRoot.c_str(), duration);
}

inline void handleServerModeDisable() {
  if (!requireAuth()) return;
  clearFlag(SysFlag::SERVER_MODE);
  serverModeEndTime = 0;
  server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"success\":true,\"isServerMode\":false}");
  logLine("📴 [Server Mode] 已關閉靜態網站託管模式");
}

inline void handleServerModeStatus() { handleStatus(); }

inline void handleEmergencyEnable() {
  if (!requireAuth()) return;
  setFlag(SysFlag::EMERGENCY_MODE);
  server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"isEmergencyMode\":true}");
  logLine("🚨 [Emergency] 急難應變模式已啟動");
}

inline void handleEmergencyDisable() {
  if (!requireAuth()) return;
  clearFlag(SysFlag::EMERGENCY_MODE);
  server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"isEmergencyMode\":false}");
  logLine("🚨 [Emergency] 急難應變模式已關閉");
}

inline void handleEmergencyStatus() { handleStatus(); }

inline void handleEmergencyGetMessages() {
  String path = "/emergency/messages.json";
  SpiLock lock;
  if (!SD.exists(path)) {
    server.send(200, "application/json; charset=utf-8", "[]");
    return;
  }
  File f = SD.open(path, FILE_READ);
  if (!f) return sendText(500, "無法讀取急難留言板");
  streamFileFast(f, "application/json; charset=utf-8");
  f.close();
}

inline void handleEmergencyPostMessage() {
  ensureDirectoryExists("/emergency");
  String sender   = server.arg("sender");
  String status   = server.arg("status");
  String location = server.arg("location");
  String text     = server.arg("text");

  sender.trim(); status.trim(); location.trim(); text.trim();
  if (sender.length() == 0) sender = "匿名民眾";
  if (status.length() == 0) status = "safe";
  if (text.length() == 0) return sendText(400, "留言內容不能為空");

  uint32_t msgId = static_cast<uint32_t>(millis() / 1000) ^ static_cast<uint32_t>(esp_random());
  if (msgId == 0) msgId = 1;

  // 1. 組裝 ESP-NOW 封包並主動向外廣播 (讓周遭節點收到)
  EspNowEmergencyPacket pkt = {};
  pkt.msgId = msgId;
  strncpy(pkt.sender, sender.c_str(), sizeof(pkt.sender) - 1);
  strncpy(pkt.location, location.c_str(), sizeof(pkt.location) - 1);
  strncpy(pkt.status, status.c_str(), sizeof(pkt.status) - 1);
  strncpy(pkt.text, text.c_str(), sizeof(pkt.text) - 1);
  pkt.hopCount = 0;

  uint8_t broadcastMac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (!esp_now_is_peer_exist(broadcastMac)) {
    esp_now_add_peer(&peerInfo);
  }
  esp_err_t sendErr = esp_now_send(broadcastMac, (uint8_t *)&pkt, sizeof(pkt));
  logf("📡 [ESP-NOW Mesh] 發送 Web 急難廣播留言 (ID:%u, 狀態:%d): %s\n", msgId, sendErr, pkt.text);

  // 2. 寫入本地 SD 卡急難留言板
  SpiLock lock;
  String existingJson = "";
  File fRead = SD.open("/emergency/messages.json", FILE_READ);
  if (fRead) {
    existingJson = fRead.readString();
    fRead.close();
  }

  existingJson.trim();
  if (!existingJson.startsWith("[") || !existingJson.endsWith("]")) existingJson = "[]";

  uint32_t nowSec = millis() / 1000;
  String newItem = "  {\"id\":" + String(msgId) + ",\"time\":" + String(nowSec) + ",\"sender\":\"" + jsonEscape(sender) + "\",\"status\":\"" + jsonEscape(status) + "\",\"location\":\"" + jsonEscape(location) + "\",\"text\":\"" + jsonEscape(text) + "\"}";

  String updatedJson;
  if (existingJson == "[]") {
    updatedJson = "[\n" + newItem + "\n]";
  } else {
    int closeIdx = existingJson.lastIndexOf(']');
    if (closeIdx != -1) {
      String prefix = existingJson.substring(0, closeIdx);
      prefix.trim();
      if (prefix == "[") updatedJson = "[\n" + newItem + "\n]";
      else updatedJson = prefix + ",\n" + newItem + "\n]";
    } else {
      updatedJson = "[\n" + newItem + "\n]";
    }
  }

  File fWrite = SD.open("/emergency/messages.json", FILE_WRITE);
  if (fWrite) {
    fWrite.print(updatedJson);
    fWrite.flush();
    fWrite.close();
    server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"success\":true,\"msgId\":" + String(msgId) + "}");
  } else {
    sendText(500, "無法寫入 SD 留言板");
  }
}

inline void handleEmergencyMap() {
  String mapPath = "/emergency/map.png";
  SpiLock lock;
  if (!SD.exists(mapPath)) mapPath = "/emergency/map.jpg";

  if (SD.exists(mapPath)) {
    File f = SD.open(mapPath, FILE_READ);
    if (f) {
      server.sendHeader("Cache-Control", "max-age=3600");
      streamFileFast(f, getMIMEType(mapPath));
      f.close();
      return;
    }
  }
  sendText(404, "尚未建立自訂 SD 急救地圖 (/emergency/map.png)");
}

inline void handleEspNowStatus() {
  server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"receivedCount\":" + String(totalEspNowReceived) + "}");
}

inline void handleUploadStatus() {
  if (!requireAuth()) return;
  String path = normalizePath(server.arg("path"));
  size_t existingSize = 0;
  SpiLock lock;
  if (SD.exists(path)) {
    File f = SD.open(path, FILE_READ);
    if (f) { existingSize = f.size(); f.close(); }
  }
  server.send(200, "application/json; charset=utf-8", "{\"path\":\"" + jsonEscape(path) + "\",\"size\":" + String(existingSize) + "}");
}

inline void handleUploadChunk() {
  if (!requireAuth()) return;
  String path = normalizePath(server.arg("path"));
  size_t offset = server.arg("offset").toInt();
  ensureDirectoryExists(parentPath(path));
  SpiLock lock;
  File f = SD.open(path, offset == 0 ? FILE_WRITE : FILE_APPEND);
  if (f) {
    WiFiClient client = server.client();
    uint8_t buf[512];
    while (client.available()) {
      size_t len = client.read(buf, sizeof(buf));
      f.write(buf, len);
    }
    f.flush();
    f.close();
    sendText(200, "OK");
  } else {
    sendText(500, "寫入 Chunk 失敗");
  }
}

inline void handleVersionsList() {
  ensureDirectoryExists("/.versions");
  SpiLock lock;
  File dir = SD.open("/.versions");
  String json;
  json.reserve(512);
  json = "[";
  bool first = true;
  if (dir && dir.isDirectory()) {
    File entry = dir.openNextFile();
    while (entry) {
      String fname = leafName(String(entry.name()));
      if (!first) json += ",";
      first = false;
      json += "{\"name\":\"" + jsonEscape(fname) + "\",\"path\":\"" + jsonEscape(joinPath("/.versions", fname)) + "\",\"size\":" + String(entry.size()) + "}";
      entry.close();
      entry = dir.openNextFile();
    }
    dir.close();
  }
  json += "]";
  server.send(200, "application/json; charset=utf-8", json);
}

inline void handleVersionsRestore() {
  if (!requireAuth()) return;
  String verFile = server.arg("verFile");
  if (verFile.length() == 0) verFile = server.arg("backupPath");
  verFile = normalizePath(verFile);

  String targetPath = server.arg("targetPath");
  if (targetPath.length() == 0) targetPath = server.arg("target");
  targetPath = normalizePath(targetPath);

  SpiLock lock;
  if (!SD.exists(verFile)) return sendText(404, "備份檔案不存在");
  createFileVersionBackup(targetPath);
  File src = SD.open(verFile, FILE_READ);
  File dst = SD.open(targetPath, FILE_WRITE);
  if (src && dst) {
    uint8_t buf[512];
    while (src.available()) {
      size_t len = src.read(buf, sizeof(buf));
      dst.write(buf, len);
    }
    src.close();
    dst.flush();
    dst.close();
    server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"success\":true,\"message\":\"還原成功\"}");
  } else {
    if (src) src.close();
    if (dst) dst.close();
    server.send(500, "application/json; charset=utf-8", "{\"status\":\"error\",\"success\":false,\"message\":\"還原失敗\"}");
  }
}

inline void handleAiBenchmark() {
  if (!requireAuth()) return;
  auto startUs = micros();
  volatile float a = 1.0001f, b = 1.0002f, sum = 0.0f;
  constexpr int ITERS = 200000;
  for (int i = 0; i < ITERS; i++) {
    sum += a * b + static_cast<float>(i) * 0.00001f;
  }
  auto durationUs = micros() - startUs;
  if (durationUs == 0) durationUs = 1;
  auto mflops = (2.0f * static_cast<float>(ITERS)) / static_cast<float>(durationUs);
  server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"mflops\":" + String(mflops, 2) + ",\"durationUs\":" + String(durationUs) + ",\"iterations\":" + String(ITERS) + "}");
}

inline void handleFactoryReset() {
  if (!requireAuth()) return;
  auto confirm = server.arg("confirm");
  if (confirm != "CONFIRM_RESET") return sendText(400, "驗證失敗");

  Preferences prefs;
  prefs.begin("sys_cfg", false);
  prefs.clear();
  prefs.end();

  if (SD.cardType() != CARD_NONE) {
    SpiLock lock;
    if (SD.exists("/config.json")) {
      SD.remove("/config.json");
    }
  }

  sendText(200, "OK");
  yield();
  ESP.restart();
}

inline void handleUndo() {
  if (!requireAuth()) return;
  String trashPath = normalizePath(server.arg("path"));
  String origPath  = normalizePath(server.arg("original"));
  if (trashPath.length() == 0 || origPath.length() == 0) {
    return sendText(400, "無效的路徑參數");
  }

  SpiLock lock;
  if (SD.exists(trashPath)) {
    ensureDirectoryExists(parentPath(origPath));
    if (SD.rename(trashPath, origPath)) {
      sendText(200, "OK");
    } else {
      sendText(500, "還原失敗");
    }
  } else {
    sendText(404, "回收站項目不存在");
  }
}

inline void handleEmptyTrash() {
  if (!requireAuth()) return;
  if (SD.exists("/.trash")) {
    removeRecursive("/.trash");
    ensureDirectoryExists("/.trash");
  }
  server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"success\":true,\"message\":\"回收站已清空\"}");
}

inline void handleSysConfigGet() {
  if (!requireAuth()) return;
  String json = "{\"status\":\"ok\"";
  json += ",\"wifi_ssid\":\"" + jsonEscape(runtimeWifiSsid) + "\"";
  json += ",\"ap_ssid\":\"" + jsonEscape(runtimeApSsid) + "\"";
  json += ",\"web_user\":\"" + jsonEscape(runtimeWebUser) + "\"";
  json += ",\"napt\":" + String(hasFlag(SysFlag::NAPT_ENABLED) ? "true" : "false");
  json += "}";
  server.send(200, "application/json; charset=utf-8", json);
}

inline void handleSysConfigPost() {
  if (!requireAuth()) return;
  String wifiSsid = server.arg("wifi_ssid");
  String wifiPass = server.arg("wifi_pass");
  String apSsid   = server.arg("ap_ssid");
  String apPass   = server.arg("ap_pass");
  String webUser  = server.arg("web_user");
  String webPass  = server.arg("web_pass");

  saveRuntimeConfig(wifiSsid, wifiPass, apSsid, apPass, webUser, webPass);
  server.send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"success\":true,\"message\":\"設定已成功同步儲存！\"}");
}

inline void handleOtaDone() {
  if (!requireAuth()) return;
  if (Update.hasError()) {
    sendText(500, "OTA 更新失敗");
  } else {
    sendText(200, "OTA 更新成功，重啟中...");
    delay(1000);
    ESP.restart();
  }
}

inline void handleOtaUpload() {
  if (!isClientAuthenticated()) return;
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      logf("OTA 韌體刷寫成功: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

inline void handleTouchDiagnostic() {
  String json = "{";
  json += "\"irq_pin_low\":" + String(lastIrqState ? "true" : "false") + ",";
  json += "\"raw_x\":" + String(lastRawX) + ",";
  json += "\"raw_y\":" + String(lastRawY) + ",";
  json += "\"z1_pressure\":" + String(lastZ1) + ",";
  json += "\"screen_x\":" + String(currentTouch.x) + ",";
  json += "\"screen_y\":" + String(currentTouch.y) + ",";
  json += "\"is_pressed\":" + String(currentTouch.isPressed ? "true" : "false") + ",";
  json += "\"current_page\":" + String(static_cast<int>(currentPage));
  json += "}";
  server.send(200, "application/json; charset=utf-8", json);
}

inline void handleWiringDiagnostic() {
  WiringTestResult r = runWiringDiagnostic();
  int passCount = static_cast<int>(r.lcdSpi) + r.lcdCs + r.lcdDc + r.lcdRst + r.touchSpi + r.touchCs + r.touchIrq + r.sdCard + r.irReceiver + r.micPins;

  float tempVal = 42.0f;
#ifdef CONFIG_IDF_TARGET_ESP32S3
  tempVal = temperatureRead();
#endif

  String json;
  json.reserve(1024);
  json = "{";

  // Wiring test
  json += "\"wiring\":{";
  json += "\"lcd_spi\":" + String(r.lcdSpi ? "true" : "false") + ",";
  json += "\"lcd_cs\":" + String(r.lcdCs ? "true" : "false") + ",";
  json += "\"lcd_dc\":" + String(r.lcdDc ? "true" : "false") + ",";
  json += "\"lcd_rst\":" + String(r.lcdRst ? "true" : "false") + ",";
  json += "\"touch_spi\":" + String(r.touchSpi ? "true" : "false") + ",";
  json += "\"touch_cs\":" + String(r.touchCs ? "true" : "false") + ",";
  json += "\"touch_irq\":" + String(r.touchIrq ? "true" : "false") + ",";
  json += "\"sd_card\":" + String(r.sdCard ? "true" : "false") + ",";
  json += "\"ir_rx\":" + String(r.irReceiver ? "true" : "false") + ",";
  json += "\"mic_pins\":" + String(r.micPins ? "true" : "false") + ",";
  json += "\"pass\":" + String(passCount) + ",\"total\":10},";

  // Touch telemetry
  json += "\"touch\":{";
  json += "\"raw_x\":" + String(lastRawX) + ",";
  json += "\"raw_y\":" + String(lastRawY) + ",";
  json += "\"z1\":" + String(lastZ1) + ",";
  json += "\"irq\":" + String(lastIrqState ? "true" : "false") + ",";
  json += "\"screen_x\":" + String(currentTouch.x) + ",";
  json += "\"screen_y\":" + String(currentTouch.y) + ",";
  json += "\"pressed\":" + String(currentTouch.isPressed ? "true" : "false") + "},";

  // System
  json += "\"system\":{";
  json += "\"temp\":" + String(tempVal, 1) + ",";
  json += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"uptime\":" + String(millis() / 1000) + ",";
  json += "\"wifi\":" + String(WiFi.isConnected() ? "true" : "false") + ",";
  if (WiFi.isConnected()) {
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  }
  json += "\"page\":" + String(static_cast<int>(currentPage)) + "},";

  // Pin Map
  json += "\"pins\":{";
  json += "\"mosi\":" + String(SD_MOSI) + ",\"miso\":" + String(SD_MISO) + ",\"sck\":" + String(SD_SCK) + ",";
  json += "\"sd_cs\":" + String(SD_CS) + ",\"lcd_cs\":" + String(LCD_CS) + ",\"lcd_dc\":" + String(LCD_DC) + ",";
  json += "\"lcd_rst\":" + String(LCD_RST) + ",\"tch_cs\":" + String(TOUCH_CS) + ",\"tch_irq\":" + String(TOUCH_IRQ) + ",";
  json += "\"ir_tx\":" + String(IR_TX_PIN) + ",\"ir_rx\":" + String(IR_RX_PIN) + ",";
  json += "\"mic_sd\":" + String(MIC_I2S_SD) + ",\"mic_sck\":" + String(MIC_I2S_SCK) + ",\"mic_ws\":" + String(MIC_I2S_WS);
  json += "}}";

  server.send(200, "application/json; charset=utf-8", json);
}

inline void handleNotFound() {
  String uri = server.uri();
  if (uri == "/generate_204" || uri == "/gen_204" || uri.endsWith("/nui") || uri.endsWith("/redirect") || uri == "/canonical.html") {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
    return;
  }

  // 若開啟了 Server Mode，嘗試自自訂目錄尋找相應靜態資源 (CSS, JS, 圖片, HTML)
  if (hasFlag(SysFlag::SERVER_MODE) && SD.cardType() != CARD_NONE) {
    String subPath = normalizePath(joinPath(serverModeRoot, uri));
    SpiLock lock;
    if (SD.exists(subPath)) {
      File f = SD.open(subPath, FILE_READ);
      if (f && !f.isDirectory()) {
        streamFileFast(f, getMIMEType(subPath));
        f.close();
        return;
      }
    }
  }

  handleIndex();
}

#endif // API_HANDLERS_H
