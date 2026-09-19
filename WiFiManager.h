#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "Config.h"

extern DNSServer captiveDns;

inline void startWiFi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);               // 關閉 Modem Sleep 以獲取最大 Wi-Fi 吞吐量與極低連線延遲
  WiFi.setHostname("sdserver");
  WiFi.setTxPower(WIFI_POWER_19_5dBm); // 提升 Wi-Fi 射頻發射功率至最高效能模式

  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  if (!WiFi.softAP(runtimeApSsid.c_str(), runtimeApPassword.c_str())) {
    logLine("[2/8] Wi-Fi 熱點…… ❌ 建立失敗！請重新開機再試一次。");
  } else {
    logf("[2/8] Wi-Fi 熱點…… ✅ 已建立「%s」\n", runtimeApSsid.c_str());
    // 啟動 Captive Portal DNS 伺服器 (將所有域名解析導向 192.168.4.1 強制登入頁)
    captiveDns.start(53, "*", WiFi.softAPIP());
  }

  if (MDNS.begin("sdserver")) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ftp", "tcp", 21);
    logLine("🌐 [mDNS] 區域網路主機名廣播就緒: http://sdserver.local");
  }

  if (runtimeWifiSsid != "YOUR_WIFI_NAME" && runtimeWifiSsid.length() > 0) {
    logf("[2/8] 連線家用 Wi-Fi「%s」", runtimeWifiSsid.c_str());
    WiFi.begin(runtimeWifiSsid.c_str(), runtimeWifiPassword.c_str());
    uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startMs < 3500) {
      delay(200);
      if ((millis() - startMs) % 800 < 200) logf(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      setFlag(SysFlag::WAS_CONNECTED);
      logf(" ✅ 成功！IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
      logLine(" 📡 背景自動連線中（不阻塞開機）");
    }
  }
}

inline void printWelcomeGuide() {
  logLine("");
  logLine("================================================");
  logLine("  🎉 開機完成！系統連線方式如下：");
  logLine("------------------------------------------------");
  logf("  ▶ 手機或電腦連線 Wi-Fi「%s」（密碼：%s）\n", runtimeApSsid.c_str(), runtimeApPassword.c_str());
  logf("    用瀏覽器開啟 → http://%s\n", WiFi.softAPIP().toString().c_str());
  if (WiFi.status() == WL_CONNECTED) {
    logLine("  ▶ 或在同一區域網路內以瀏覽器開啟：");
    logf("    → http://%s（或 http://sdserver.local）\n", WiFi.localIP().toString().c_str());
  }
  logLine("  ▶ 用 USB-C 連接線接電腦，可作為高速隨身碟讀寫");
  logLine("  ▶ FTP 傳檔：Port 21（帳號任意，密碼＝網頁密碼）");
  logLine("================================================");
  logLine("");
}

inline void watchWiFiStatus() {
  static uint32_t lastCheck = 0;
  static bool firstRun = true;
  static bool wasConnected = false;
  static uint32_t lastRetryMs = 0;

  if (millis() - lastCheck < 1000) return;
  lastCheck = millis();

  bool nowConnected = (WiFi.status() == WL_CONNECTED);
  if (firstRun) {
    firstRun = false;
    wasConnected = nowConnected;
    return;
  }

  if (!wasConnected && nowConnected) {
    setFlag(SysFlag::WAS_CONNECTED);
    logf("📶 已成功連上 Wi-Fi「%s」！\n", WiFi.SSID().c_str());
    logf("   可用瀏覽器開啟 → http://%s\n", WiFi.localIP().toString().c_str());
  } else if (wasConnected && !nowConnected) {
    logLine("📴 與外部 Wi-Fi 的連線已中斷，背景自動重連中。");
  } else if (!nowConnected && (millis() - lastRetryMs > 12000)) {
    lastRetryMs = millis();
    if (runtimeWifiSsid != "YOUR_WIFI_NAME" && runtimeWifiSsid.length() > 0) {
      WiFi.reconnect();
    }
  }
  wasConnected = nowConnected;
}

inline void checkThermalGuard() {
  static uint32_t lastThermalCheck = 0;
  if (millis() - lastThermalCheck < 3000) return;
  lastThermalCheck = millis();

#ifdef CONFIG_IDF_TARGET_ESP32S3
  float temp = temperatureRead();
  if (temp > 68.0f && !hasFlag(SysFlag::THERMAL_THROTTLED)) {
    setCpuFrequencyMhz(80); // 溫度過高時動態降至 80MHz 低溫散熱模式
    WiFi.setTxPower(WIFI_POWER_13dBm);
    setFlag(SysFlag::THERMAL_THROTTLED);
    logf("🌡️ 晶片溫度守護：目前 %.1f°C，啟動低溫節能散熱模式（240→80MHz）。\n", temp);
  } else if (temp < 58.0f && hasFlag(SysFlag::THERMAL_THROTTLED)) {
    setCpuFrequencyMhz(240); // 溫度回落至安全區間，恢復 240MHz 滿血時脈
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    clearFlag(SysFlag::THERMAL_THROTTLED);
    logf("🌡️ 晶片溫度回落安全區間（%.1f°C），恢復 240MHz 滿血時脈。\n", temp);
  }
#endif
}

// ---------------------------------------------------------------------------
// 網路分享（NAPT 中繼 + 高效 DNS 轉發）：讓熱點終端透過本機外部 Wi-Fi 上網
// ---------------------------------------------------------------------------
static WiFiUDP g_dnsRelayUdp;
static WiFiUDP g_dnsUpstreamUdp;
static bool g_dnsRelayStarted = false;

inline void initDnsRelay() {
  if (!g_dnsRelayStarted) {
    g_dnsRelayUdp.begin(53);
    g_dnsUpstreamUdp.begin(0); // 常駐的上游轉發 Socket，徹底避免資源洩漏
    g_dnsRelayStarted = true;
  }
}

inline void handleDnsRelay() {
  if (!g_dnsRelayStarted || !hasFlag(SysFlag::NAPT_ENABLED) || WiFi.status() != WL_CONNECTED) {
    return;
  }

  int packetSize = g_dnsRelayUdp.parsePacket();
  if (packetSize > 0 && packetSize < 512) {
    uint8_t dnsBuffer[512];
    int len = g_dnsRelayUdp.read(dnsBuffer, sizeof(dnsBuffer));
    IPAddress clientIp = g_dnsRelayUdp.remoteIP();
    uint16_t clientPort = g_dnsRelayUdp.remotePort();

    IPAddress upstreamDns = WiFi.dnsIP(0);
    if (upstreamDns == IPAddress(0, 0, 0, 0)) {
      upstreamDns = IPAddress(8, 8, 8, 8);
    }

    g_dnsUpstreamUdp.beginPacket(upstreamDns, 53);
    g_dnsUpstreamUdp.write(dnsBuffer, len);
    g_dnsUpstreamUdp.endPacket();

    uint32_t startMs = millis();
    while (millis() - startMs < 300) {
      int respSize = g_dnsUpstreamUdp.parsePacket();
      if (respSize > 0 && respSize < 512) {
        uint8_t respBuffer[512];
        int respLen = g_dnsUpstreamUdp.read(respBuffer, sizeof(respBuffer));
        g_dnsRelayUdp.beginPacket(clientIp, clientPort);
        g_dnsRelayUdp.write(respBuffer, respLen);
        g_dnsRelayUdp.endPacket();
        break;
      }
      yield();
    }
  }
}

inline void enableNAPTRepeater() {
#if IP_NAPT
  if (WiFi.status() != WL_CONNECTED) {
    logLine("⚠️ 尚未連上外部 Wi-Fi，無法開啟網路分享（中繼）。");
    return;
  }

  logLine("──── 網路分享（Wi-Fi 中繼）啟動中 ────");
  logf("   外部位址：%s ／ 閘道：%s\n", WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str());

  uint32_t ap_ip = static_cast<uint32_t>(WiFi.softAPIP());

  // 關閉 Captive Portal DNS 轉發，切換為 NAPT 外網轉發
  captiveDns.stop();

  LOCK_TCPIP_CORE();
  ip_napt_enable(ap_ip, 1);
  UNLOCK_TCPIP_CORE();

  initDnsRelay();
  setFlag(SysFlag::NAPT_ENABLED);
  logLine("✅ 網路分享（Wi-Fi NAPT 中繼與 DNS 轉發）啟動成功！");
#else
  logLine("⚠️ 目前編譯環境不支援 IP_NAPT。");
#endif
}

inline void disableNAPTRepeater() {
#if IP_NAPT
  uint32_t ap_ip = static_cast<uint32_t>(WiFi.softAPIP());
  LOCK_TCPIP_CORE();
  ip_napt_enable(ap_ip, 0);
  UNLOCK_TCPIP_CORE();
  clearFlag(SysFlag::NAPT_ENABLED);

  // 重新啟動 Captive Portal DNS
  captiveDns.start(53, "*", WiFi.softAPIP());
  logLine("📴 網路分享（Wi-Fi 中繼）已關閉，已恢復 Captive Portal 強制門戶。");
#else
  clearFlag(SysFlag::NAPT_ENABLED);
#endif
}

inline void handleCaptivePortalDNS() {
  if (hasFlag(SysFlag::NAPT_ENABLED) && WiFi.status() == WL_CONNECTED) {
    handleDnsRelay();
  } else {
    captiveDns.processNextRequest();
  }
}

#endif // WIFI_MANAGER_H
