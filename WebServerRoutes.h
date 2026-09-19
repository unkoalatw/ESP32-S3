#ifndef WEBSERVER_ROUTES_H
#define WEBSERVER_ROUTES_H

#include "Config.h"
#include "ApiHandlers.h"

// ---------------------------------------------------------------------------
// WebServer HTTP 路由註冊與按需加載映射
// ---------------------------------------------------------------------------
inline void initWebServer() {
  const char *headerKeys[] = { "Cookie", "X-Auth-Token", "User-Agent", "Range", "If-None-Match" };
  server.collectHeaders(headerKeys, 5);

  // 1. 鑑權與 Session 路由
  server.on("/api/login",       HTTP_POST, []() { handleLogin(); });
  server.on("/api/logout",      HTTP_POST, []() { handleLogout(); });
  server.on("/api/auth/status", HTTP_GET,  []() { handleAuthStatus(); });

  // 2. 靜態資源與 SPA 單頁入口
  server.on("/",              HTTP_GET, []() { handleIndex(); });
  server.on("/manifest.json", HTTP_GET, []() { handleManifestJson(); });
  server.on("/sw.js",         HTTP_GET, []() { handleServiceWorkerJs(); });
  server.on("/favicon.ico",   HTTP_GET, []() { handleFavicon(); });

  // 3. 系統狀態與日誌
  server.on("/api/status",     HTTP_GET,  []() { handleStatus(); });
  server.on("/api/diagnose",   HTTP_GET,  []() { handleDiagnose(); });
  server.on("/api/logs",       HTTP_GET,  []() { handleLogs(); });
  server.on("/api/logs/clear", HTTP_POST, []() { handleLogsClear(); });
  server.on("/api/logs/clear", HTTP_GET,  []() { handleLogsClear(); });

  // 4. 檔案系統中心與上傳/下載/回收站
  server.on("/api/list",        HTTP_GET,  []() { handleList(); });
  server.on("/download",        HTTP_GET,  []() { handleDownload(); });
  server.on("/view",            HTTP_GET,  []() { handleView(); });
  server.on("/api/mkdir",       HTTP_POST, []() { handleMkdir(); });
  server.on("/api/rename",      HTTP_POST, []() { handleRename(); });
  server.on("/api/move",        HTTP_POST, []() { handleMove(); });
  server.on("/api/delete",      HTTP_POST, []() { handleDelete(); });
  server.on("/api/undo",        HTTP_POST, []() { handleUndo(); });
  server.on("/api/empty-trash", HTTP_POST, []() { handleEmptyTrash(); });
  server.on("/upload",          HTTP_POST, []() { handleUploadDone(); }, []() { handleUploadData(); });
  server.on("/api/upload/status", HTTP_GET,  []() { handleUploadStatus(); });
  server.on("/api/upload/chunk",  HTTP_POST, []() { handleUploadChunk(); });

  // 5. 線上文字編輯與 ZIP 解包
  server.on("/api/read",  HTTP_GET,  []() { handleRead(); });
  server.on("/api/write", HTTP_POST, []() { handleWrite(); });
  server.on("/api/unzip", HTTP_POST, []() { handleUnzip(); });

  // 6. Wi-Fi 管理與 NAPT 中繼
  server.on("/api/wifi/scan",             HTTP_GET,  []() { handleWiFiScan(); });
  server.on("/api/wifi/connect",          HTTP_POST, []() { handleWiFiConnect(); });
  server.on("/api/wifi/disconnect",       HTTP_POST, []() { handleWiFiDisconnect(); });
  server.on("/api/wifi/status",           HTTP_GET,  []() { handleWiFiStatus(); });
  server.on("/api/wifi/repeater/enable",  HTTP_POST, []() { handleRepeaterEnable(); });
  server.on("/api/wifi/repeater/disable", HTTP_POST, []() { handleRepeaterDisable(); });
  server.on("/api/repeater",              HTTP_POST, []() { handleRepeaterPost(); });

  // 7. 萬用紅外線遙控中心
  server.on("/api/ir/send",    HTTP_POST, []() { handleIrSend(); });
  server.on("/api/ir/send",    HTTP_GET,  []() { handleIrSend(); });
  server.on("/api/ir/learn",   HTTP_GET,  []() { handleIrLearn(); });
  server.on("/api/ir/signals", HTTP_GET,  []() { handleIrListSignals(); });
  server.on("/api/ir/presets", HTTP_GET,  []() { handleIrGetPresets(); });
  server.on("/api/ir/presets", HTTP_POST, []() { handleIrSetPresets(); });
  server.on("/api/ir/save",    HTTP_POST, []() { handleIrSaveSignal(); });
  server.on("/api/ir/rename",  HTTP_POST, []() { handleIrRenameSignal(); });
  server.on("/api/ir/delete",  HTTP_POST, []() { handleIrDeleteSignal(); });

  // 8. I2S 數位麥克風錄音室
  server.on("/api/recorder/start",  HTTP_POST, []() { handleRecordStart(); });
  server.on("/api/recorder/stop",   HTTP_POST, []() { handleRecordStop(); });
  server.on("/api/recorder/status", HTTP_GET,  []() { handleRecordStatus(); });
  server.on("/api/recorder/list",   HTTP_GET,  []() { handleRecordingsList(); });

  // 9. 生產力：Wiki / 電子書 / 氣象站
  server.on("/api/wiki/search",       HTTP_GET,  []() { handleWikiSearch(); });
  server.on("/api/books/list",        HTTP_GET,  []() { handleBooksList(); });
  server.on("/api/weather/current",   HTTP_GET,  []() { handleWeatherCurrent(); });
  server.on("/api/weather/list",      HTTP_GET,  []() { handleWeatherList(); });
  server.on("/api/weather/log-now",   HTTP_POST, []() { handleWeatherLogNow(); });
  server.on("/api/weather/interval",  HTTP_POST, []() { handleWeatherInterval(); });

  // 10. 系統託管與急難門戶
  server.on("/api/server-mode/enable",  HTTP_POST, []() { handleServerModeEnable(); });
  server.on("/api/server-mode/disable", HTTP_POST, []() { handleServerModeDisable(); });
  server.on("/api/server-mode/status",  HTTP_GET,  []() { handleServerModeStatus(); });
  server.on("/api/emergency/enable",         HTTP_POST, []() { handleEmergencyEnable(); });
  server.on("/api/emergency/disable",        HTTP_POST, []() { handleEmergencyDisable(); });
  server.on("/api/emergency/status",         HTTP_GET,  []() { handleEmergencyStatus(); });
  server.on("/api/emergency/messages",       HTTP_GET,  []() { handleEmergencyGetMessages(); });
  server.on("/api/emergency/post-message",   HTTP_POST, []() { handleEmergencyPostMessage(); });
  server.on("/api/emergency/map",            HTTP_GET,  []() { handleEmergencyMap(); });
  server.on("/api/emergency/espnow/status",  HTTP_GET,  []() { handleEspNowStatus(); });

  // 11. 系統組態、OTA 韌體更新與版本管理
  server.on("/api/sys-config",        HTTP_GET,  []() { handleSysConfigGet(); });
  server.on("/api/sys-config",        HTTP_POST, []() { handleSysConfigPost(); });
  server.on("/api/ota",               HTTP_POST, []() { handleOtaDone(); }, []() { handleOtaUpload(); });
  server.on("/api/versions/list",     HTTP_GET,  []() { handleVersionsList(); });
  server.on("/api/versions/restore",  HTTP_POST, []() { handleVersionsRestore(); });
  server.on("/api/ai/benchmark",      HTTP_GET,  []() { handleAiBenchmark(); });
  server.on("/api/factory-reset",     HTTP_POST, []() { handleFactoryReset(); });

  // 12. 即時觸控與接線診斷
  server.on("/api/touch/diagnostic",  HTTP_GET,  []() { handleTouchDiagnostic(); });
  server.on("/api/wiring/diag",       HTTP_GET,  []() { handleWiringDiagnostic(); });

  // 404 與強制門戶處理
  server.onNotFound([]() { handleNotFound(); });

  server.begin();
  logLine("[4/8] 網頁伺服器…… ✅ 啟動完成");
}

#endif // WEBSERVER_ROUTES_H
