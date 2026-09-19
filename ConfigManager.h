#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "Config.h"
#include <Preferences.h>

// ---------------------------------------------------------------------------
// 系統動態組態管理器 (支援 NVS 快閃記憶體與 SD 卡雙向同步)
// ---------------------------------------------------------------------------

namespace ConfigHelper {
  // 健全、輕量且非阻塞的 JSON 鍵值提取器
  inline String extractJsonVal(const String &str, const String &key) {
    String pattern = "\"" + key + "\"";
    int kIdx = str.indexOf(pattern);
    if (kIdx == -1) return "";
    int colon = str.indexOf(':', kIdx + pattern.length());
    if (colon == -1) return "";
    int startQuote = str.indexOf('"', colon + 1);
    if (startQuote == -1) return "";
    int endQuote = str.indexOf('"', startQuote + 1);
    if (endQuote == -1) return "";
    return str.substring(startQuote + 1, endQuote);
  }

  inline String buildConfigJson(const String &wifiSsid, const String &wifiPass,
                                const String &apSsid, const String &apPass,
                                const String &webUser, const String &webPass) {
    String out;
    out.reserve(320);
    out += "{\n";
    out += "  \"wifi_ssid\": \"" + jsonEscape(wifiSsid) + "\",\n";
    out += "  \"wifi_pass\": \"" + jsonEscape(wifiPass) + "\",\n";
    out += "  \"ap_ssid\": \""   + jsonEscape(apSsid)   + "\",\n";
    out += "  \"ap_pass\": \""   + jsonEscape(apPass)   + "\",\n";
    out += "  \"web_user\": \""  + jsonEscape(webUser)  + "\",\n";
    out += "  \"web_pass\": \""  + jsonEscape(webPass)  + "\"\n";
    out += "}\n";
    return out;
  }
}

inline void initRuntimeConfig() {
  Preferences prefs;
  prefs.begin("sys_cfg", false);

  String nvsWifiSsid = prefs.getString("wifi_ssid", "");
  String nvsWifiPass = prefs.getString("wifi_pass", "");
  String nvsApSsid   = prefs.getString("ap_ssid", "");
  String nvsApPass   = prefs.getString("ap_pass", "");
  String nvsWebUser  = prefs.getString("web_user", "");
  String nvsWebPass  = prefs.getString("web_pass", "");

  bool sdConfigFound = false;
  String sdWifiSsid, sdWifiPass, sdApSsid, sdApPass, sdWebUser, sdWebPass;

  // 1. 檢查 SD 卡是否有 /config.json (受 SpiLock 保護)
  if (SD.cardType() != CARD_NONE) {
    SpiLock lock;
    if (SD.exists("/config.json")) {
      File f = SD.open("/config.json", FILE_READ);
      if (f) {
        String json = f.readString();
        f.close();

        sdWifiSsid = ConfigHelper::extractJsonVal(json, "wifi_ssid");
        sdWifiPass = ConfigHelper::extractJsonVal(json, "wifi_pass");
        sdApSsid   = ConfigHelper::extractJsonVal(json, "ap_ssid");
        sdApPass   = ConfigHelper::extractJsonVal(json, "ap_pass");
        sdWebUser  = ConfigHelper::extractJsonVal(json, "web_user");
        sdWebPass  = ConfigHelper::extractJsonVal(json, "web_pass");

        if (sdWebPass.length() > 0 || sdWifiSsid.length() > 0 || sdApSsid.length() > 0) {
          sdConfigFound = true;
        }
      }
    }
  }

  // 2. 雙向同步裁決 (Priority: SD Card > NVS Flash > Factory Defaults)
  if (sdConfigFound) {
    runtimeWifiSsid     = (sdWifiSsid.length() > 0) ? sdWifiSsid : FACTORY_WIFI_SSID;
    runtimeWifiPassword = (sdWifiPass.length() > 0) ? sdWifiPass : FACTORY_WIFI_PASSWORD;
    runtimeApSsid       = (sdApSsid.length() > 0)   ? sdApSsid   : FACTORY_AP_SSID;
    runtimeApPassword   = (sdApPass.length() > 0)   ? sdApPass   : FACTORY_AP_PASSWORD;
    runtimeWebUser      = (sdWebUser.length() > 0)  ? sdWebUser  : FACTORY_WEB_USER;
    runtimeWebPassword  = (sdWebPass.length() > 0)  ? sdWebPass  : FACTORY_WEB_PASSWORD;

    // 同步寫入 NVS，確保換 SD 卡後晶片內依然保留配置
    prefs.putString("wifi_ssid", runtimeWifiSsid);
    prefs.putString("wifi_pass", runtimeWifiPassword);
    prefs.putString("ap_ssid",   runtimeApSsid);
    prefs.putString("ap_pass",   runtimeApPassword);
    prefs.putString("web_user",  runtimeWebUser);
    prefs.putString("web_pass",  runtimeWebPassword);
    logLine("⚙️ [Config] 成功從 SD 卡載入設定，並同步寫入晶片 NVS Flash！");
  } else if (nvsWebPass.length() > 0 || nvsWifiSsid.length() > 0 || nvsApSsid.length() > 0) {
    runtimeWifiSsid     = (nvsWifiSsid.length() > 0) ? nvsWifiSsid : FACTORY_WIFI_SSID;
    runtimeWifiPassword = (nvsWifiPass.length() > 0) ? nvsWifiPass : FACTORY_WIFI_PASSWORD;
    runtimeApSsid       = (nvsApSsid.length() > 0)   ? nvsApSsid   : FACTORY_AP_SSID;
    runtimeApPassword   = (nvsApPass.length() > 0)   ? nvsApPass   : FACTORY_AP_PASSWORD;
    runtimeWebUser      = (nvsWebUser.length() > 0)  ? nvsWebUser  : FACTORY_WEB_USER;
    runtimeWebPassword  = (nvsWebPass.length() > 0)  ? nvsWebPass  : FACTORY_WEB_PASSWORD;
    logLine("⚙️ [Config] 成功從晶片 NVS Flash 載入設定！");

    // 同步寫回 SD 卡（若有卡）
    if (SD.cardType() != CARD_NONE) {
      SpiLock lock;
      File f = SD.open("/config.json", FILE_WRITE);
      if (f) {
        String out = ConfigHelper::buildConfigJson(runtimeWifiSsid, runtimeWifiPassword,
                                                   runtimeApSsid, runtimeApPassword,
                                                   runtimeWebUser, runtimeWebPassword);
        f.print(out);
        f.flush();
        f.close();
        logLine("⚙️ [Config] 已自動為新 SD 卡同步生成 /config.json！");
      }
    }
  } else {
    // 全新出廠狀態：套用出廠預設值
    runtimeWifiSsid     = FACTORY_WIFI_SSID;
    runtimeWifiPassword = FACTORY_WIFI_PASSWORD;
    runtimeApSsid       = FACTORY_AP_SSID;
    runtimeApPassword   = FACTORY_AP_PASSWORD;
    runtimeWebUser      = FACTORY_WEB_USER;
    runtimeWebPassword  = FACTORY_WEB_PASSWORD;

    prefs.putString("wifi_ssid", runtimeWifiSsid);
    prefs.putString("wifi_pass", runtimeWifiPassword);
    prefs.putString("ap_ssid",   runtimeApSsid);
    prefs.putString("ap_pass",   runtimeApPassword);
    prefs.putString("web_user",  runtimeWebUser);
    prefs.putString("web_pass",  runtimeWebPassword);

    if (SD.cardType() != CARD_NONE) {
      SpiLock lock;
      if (!SD.exists("/config.json")) {
        File f = SD.open("/config.json", FILE_WRITE);
        if (f) {
          String out = ConfigHelper::buildConfigJson(runtimeWifiSsid, runtimeWifiPassword,
                                                     runtimeApSsid, runtimeApPassword,
                                                     runtimeWebUser, runtimeWebPassword);
          f.print(out);
          f.flush();
          f.close();
        }
      }
    }
    logLine("⚙️ [Config] 初始化出廠預設值完成（已同步至 NVS 與 SD 卡）！");
  }
  prefs.end();
}

inline void saveRuntimeConfig(const String &wifiSsid, const String &wifiPass,
                              const String &apSsid, const String &apPass,
                              const String &webUser, const String &webPass) {
  if (wifiSsid.length() > 0) runtimeWifiSsid     = wifiSsid;
  if (wifiPass.length() > 0) runtimeWifiPassword = wifiPass;
  if (apSsid.length() > 0)   runtimeApSsid       = apSsid;
  if (apPass.length() > 0)   runtimeApPassword   = apPass;
  if (webUser.length() > 0)  runtimeWebUser      = webUser;
  if (webPass.length() > 0)  runtimeWebPassword  = webPass;

  // 1. 寫入晶片 NVS
  Preferences prefs;
  prefs.begin("sys_cfg", false);
  prefs.putString("wifi_ssid", runtimeWifiSsid);
  prefs.putString("wifi_pass", runtimeWifiPassword);
  prefs.putString("ap_ssid",   runtimeApSsid);
  prefs.putString("ap_pass",   runtimeApPassword);
  prefs.putString("web_user",  runtimeWebUser);
  prefs.putString("web_pass",  runtimeWebPassword);
  prefs.end();

  // 2. 寫入 SD 卡 /config.json
  if (SD.cardType() != CARD_NONE) {
    SpiLock lock;
    File f = SD.open("/config.json", FILE_WRITE);
    if (f) {
      String out = ConfigHelper::buildConfigJson(runtimeWifiSsid, runtimeWifiPassword,
                                                 runtimeApSsid, runtimeApPassword,
                                                 runtimeWebUser, runtimeWebPassword);
      f.print(out);
      f.flush();
      f.close();
    }
  }
  logLine("💾 [Config] 動態設定已同步儲存至 NVS Flash 與 SD 卡 /config.json！");
}

#endif // CONFIG_MANAGER_H
