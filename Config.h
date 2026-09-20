#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include <stdarg.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <esp_now.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>
#include "esp_log.h"

#define IP_NAPT 1
#define IP_PORTMAP 1
#include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/ip_addr.h"
#include "lwip/lwip_napt.h"
#include "lwip/tcpip.h"
#include "esp_netif.h"

#include <atomic>
#include "index_html.h"

// ---------------------------------------------------------------------------
// 系統位元遮罩旗標 (Bitwise Math Flags - 採用 std::atomic 確保跨核心線程安全)
// ---------------------------------------------------------------------------
namespace SysFlagBit {
  constexpr uint8_t USB_MOUNTED       = 0;
  constexpr uint8_t SERVER_MODE       = 1;
  constexpr uint8_t EMERGENCY_MODE    = 2;
  constexpr uint8_t THERMAL_THROTTLED = 3;
  constexpr uint8_t NAPT_ENABLED      = 4;
  constexpr uint8_t ESPNOW_INBOX_FULL = 5;
  constexpr uint8_t WAS_CONNECTED     = 6;
  constexpr uint8_t UPLOAD_AUTH       = 7;
  constexpr uint8_t UPLOAD_OVERWRITE  = 8;
  constexpr uint8_t USB_EXCLUSIVE_LOCK = 9;
}

namespace SysFlag {
  constexpr uint32_t USB_MOUNTED        = 1UL << SysFlagBit::USB_MOUNTED;
  constexpr uint32_t SERVER_MODE        = 1UL << SysFlagBit::SERVER_MODE;
  constexpr uint32_t EMERGENCY_MODE     = 1UL << SysFlagBit::EMERGENCY_MODE;
  constexpr uint32_t THERMAL_THROTTLED  = 1UL << SysFlagBit::THERMAL_THROTTLED;
  constexpr uint32_t NAPT_ENABLED       = 1UL << SysFlagBit::NAPT_ENABLED;
  constexpr uint32_t ESPNOW_INBOX_FULL  = 1UL << SysFlagBit::ESPNOW_INBOX_FULL;
  constexpr uint32_t WAS_CONNECTED      = 1UL << SysFlagBit::WAS_CONNECTED;
  constexpr uint32_t UPLOAD_AUTH        = 1UL << SysFlagBit::UPLOAD_AUTH;
  constexpr uint32_t UPLOAD_OVERWRITE   = 1UL << SysFlagBit::UPLOAD_OVERWRITE;
  constexpr uint32_t USB_EXCLUSIVE_LOCK = 1UL << SysFlagBit::USB_EXCLUSIVE_LOCK;
}

extern std::atomic<uint32_t> systemFlags;
extern SemaphoreHandle_t g_spiMutex;

inline void initSpiMutex() {
  if (!g_spiMutex) {
    g_spiMutex = xSemaphoreCreateRecursiveMutex();
  }
}

// 健全 RAII 遞迴互斥鎖 (支援有界超時與狀態查詢)
class SpiLock {
private:
  bool _locked;
public:
  explicit SpiLock(TickType_t timeout = portMAX_DELAY) : _locked(false) {
    if (g_spiMutex) {
      _locked = (xSemaphoreTakeRecursive(g_spiMutex, timeout) == pdTRUE);
    }
  }
  ~SpiLock() {
    if (_locked && g_spiMutex) {
      xSemaphoreGiveRecursive(g_spiMutex);
      _locked = false;
    }
  }
  bool isLocked() const { return _locked; }

  SpiLock(const SpiLock &) = delete;
  SpiLock &operator=(const SpiLock &) = delete;
};

inline void setFlag(uint32_t flag) { systemFlags.fetch_or(flag, std::memory_order_relaxed); }
inline void clearFlag(uint32_t flag) { systemFlags.fetch_and(~flag, std::memory_order_relaxed); }
inline bool hasFlag(uint32_t flag) { return (systemFlags.load(std::memory_order_relaxed) & flag) != 0; }
inline bool readSysBit(uint8_t bitPos) { return (systemFlags.load(std::memory_order_relaxed) & (1UL << bitPos)) != 0; }
inline void setSysBit(uint8_t bitPos) { systemFlags.fetch_or(1UL << bitPos, std::memory_order_relaxed); }
inline void clearSysBit(uint8_t bitPos) { systemFlags.fetch_and(~(1UL << bitPos), std::memory_order_relaxed); }

// 64-bit 單調遞增系統秒數 (避免 millis() 32-bit 在 49.7 天發生 wrap 溢位)
inline uint64_t monotonicSeconds() {
  return static_cast<uint64_t>(esp_timer_get_time()) / 1000000ULL;
}

inline bool isStorageLocked() {
  return hasFlag(SysFlag::USB_EXCLUSIVE_LOCK);
}

// ---------------------------------------------------------------------------
// 出廠預設值 (Factory Defaults - 安全通用預設，首次啟動請至後台修改)
// ---------------------------------------------------------------------------
constexpr char FACTORY_WIFI_SSID[]     = "";
constexpr char FACTORY_WIFI_PASSWORD[] = "";
constexpr char FACTORY_AP_SSID[]       = "ESP32-SD-HUB";
constexpr char FACTORY_AP_PASSWORD[]   = "12345678";
constexpr char FACTORY_WEB_USER[]      = "admin";
constexpr char FACTORY_WEB_PASSWORD[]  = "admin123";

// 運行時動態設定（支援 NVS 快閃記憶體 + SD 卡雙向同步）
extern String runtimeWifiSsid;
extern String runtimeWifiPassword;
extern String runtimeApSsid;
extern String runtimeApPassword;
extern String runtimeWebUser;
extern String runtimeWebPassword;

// ---------------------------------------------------------------------------
// 硬體腳位配置 (MicroSD, TFT LCD, Touch, Audio, IR, Buttons)
// ---------------------------------------------------------------------------
constexpr int SD_CS   = 10;
constexpr int SD_MOSI = 11;
constexpr int SD_SCK  = 12;
constexpr int SD_MISO = 13;

// 實體按鈕模組 (KY-004 / Keyes 輕觸開關模組 GPIO 16 / 板載 BOOT 鍵 GPIO 0)
constexpr int BUTTON_PIN       = 16;
constexpr int ONBOARD_BOOT_PIN = 0;

// 紅外線收發腳位 (D011 TX / D012 RX)
constexpr int IR_TX_PIN = 18;  // D011 紅外線發射 (KY-005)
constexpr int IR_RX_PIN = 8;   // D012 紅外線接收 (VS1838B)

// INMP441 I2S 數位麥克風腳位
constexpr int MIC_I2S_SD  = 4;  // I2S Data Line
constexpr int MIC_I2S_SCK = 5;  // I2S Bit Clock Line
constexpr int MIC_I2S_WS  = 6;  // I2S Word Select Line

// M070 2.8 吋 TFT 液晶螢幕與 XPT2046 觸控腳位
// 【重要】Touch SPI 與 LCD / SD 共享 SPI 匯流排：
//   T_CLK (SCK)  -> GPIO 12
//   T_DIN (MOSI) -> GPIO 11
//   T_DO  (MISO) -> GPIO 13
constexpr int LCD_CS    = 9;   // M070 螢幕片選 (CS)
constexpr int LCD_DC    = 14;  // M070 資料/指令 (DC/RS)
constexpr int LCD_RST   = 21;  // M070 硬體重置 (RST)
constexpr int LCD_BL    = -1;  // M070 背光控制 (-1 表常開接 3.3V)
constexpr int TOUCH_CS  = 7;   // M070 XPT2046 觸控片選 (T_CS)
constexpr int TOUCH_IRQ = 15;  // M070 XPT2046 觸控中斷 (T_IRQ/T_PEN - 診斷可選)

constexpr uint32_t SD_SPI_FREQUENCY = 40000000UL;

constexpr uint16_t FTP_PORT      = 21;
constexpr uint16_t FTP_DATA_PORT = 2021;

extern bool g_isScreenOn;
extern uint32_t currentWikiArticle;
extern uint32_t currentWikiPage;
extern uint32_t currentEbookIndex;
extern uint32_t currentEbookPage;
extern WebServer server;
extern File uploadFile;
extern volatile bool g_isUploadingActive;
extern String uploadTarget, uploadError;
extern size_t uploadReceivedBytes;

extern uint64_t serverModeEndTime;
extern String serverModeRoot;

extern uint64_t totalBytesSent;
extern uint64_t totalBytesReceived;
extern volatile uint32_t lastUsbActivity;

// ---------------------------------------------------------------------------
// 輔助小工具函式 (Logging, Strings & JSON)
// ---------------------------------------------------------------------------
inline void logf(const char *fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print(buf);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial0.print(buf);
#endif
}

inline void logLine(const String &msg) {
  logf("%s\n", msg.c_str());
}

inline String humanSize(uint64_t bytes) {
  char buf[24];
  if (bytes >= 1073741824ULL) {
    snprintf(buf, sizeof(buf), "%.2f GB", bytes / 1073741824.0);
  } else if (bytes >= 1048576ULL) {
    snprintf(buf, sizeof(buf), "%.1f MB", bytes / 1048576.0);
  } else if (bytes >= 1024ULL) {
    snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);
  } else {
    snprintf(buf, sizeof(buf), "%u B", static_cast<unsigned>(bytes));
  }
  return String(buf);
}

// 嚴密快速的 JSON 字串跳脫（處理 Unicode、控制字元與特殊符號）
inline String jsonEscape(const String &v) {
  String out;
  out.reserve(v.length() + (v.length() >> 2) + 8);
  for (size_t i = 0; i < v.length(); ++i) {
    char c = v[i];
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"':  out += "\\\""; break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      case '\b': out += "\\b";  break;
      case '\f': out += "\\f";  break;
      default:
        if (static_cast<uint8_t>(c) < 0x20) {
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04x", static_cast<uint8_t>(c));
          out += buf;
        } else {
          out += c;
        }
        break;
    }
  }
  return out;
}

// 關閉 ESP32-S3 開發板上板載指示燈 (嚴密隔離 LCD_RST Pin 21，防止螢幕重置訊號干擾)
inline void turnOffOnboardLeds() {
#ifdef RGB_BUILTIN
  neopixelWrite(RGB_BUILTIN, 0, 0, 0);
#endif
#ifdef LED_BUILTIN
  if (LED_BUILTIN != LCD_RST && LED_BUILTIN != LCD_CS && LED_BUILTIN != LCD_DC && LED_BUILTIN != TOUCH_CS) {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
  }
#endif
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  // 常見 ESP32-S3 RGB 腳位 (GPIO 48 / 38)，排除 GPIO 21 (LCD_RST)
  neopixelWrite(48, 0, 0, 0);
  neopixelWrite(38, 0, 0, 0);
#endif
}

#endif // CONFIG_H
