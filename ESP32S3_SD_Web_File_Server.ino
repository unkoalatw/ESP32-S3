/*
  ===========================================================================
   ESP32-S3 SD Web File Server / SD HUB V3.5
   工業級全功能微型 Web 伺服器、離線資料中心與多模組邊緣主控台
  ===========================================================================
*/

#include "Config.h"
#include "SDCardManager.h"
#include "ConfigManager.h"
#include "WiFiManager.h"
#include "UsbMscManager.h"
#include "FtpServerManager.h"
#include "EmergencyMesh.h"
#include "WeatherStation.h"
#include "IrRemoteManager.h"
#include "AudioRecorderManager.h"
#include "TftDisplayManager.h"
#include "ApiHandlers.h"
#include "ButtonManager.h"
#include "WebServerRoutes.h"

// ---------------------------------------------------------------------------
// 全域實體與狀態定義（全專案唯一實體）
// ---------------------------------------------------------------------------
SemaphoreHandle_t g_spiMutex = NULL;
volatile uint32_t systemFlags = 0;
volatile uint32_t lastUsbActivity = 0;

DNSServer captiveDns;
WebServer server(80);

File uploadFile;
volatile bool g_isUploadingActive = false;
String uploadTarget = "";
String uploadError = "";
size_t uploadReceivedBytes = 0;

String runtimeWifiSsid     = FACTORY_WIFI_SSID;
String runtimeWifiPassword = FACTORY_WIFI_PASSWORD;
String runtimeApSsid       = FACTORY_AP_SSID;
String runtimeApPassword   = FACTORY_AP_PASSWORD;
String runtimeWebUser      = FACTORY_WEB_USER;
String runtimeWebPassword  = FACTORY_WEB_PASSWORD;

uint32_t serverModeEndTime = 0;
String serverModeRoot = "/html";

uint64_t totalBytesSent = 0;
uint64_t totalBytesReceived = 0;

uint32_t totalEspNowReceived = 0;
EspNowEmergencyPacket espNowInbox;

uint32_t lastWeatherLogTime = 0;
uint32_t weatherLogIntervalSec = 60;
uint32_t totalWeatherLogCount = 0;

IrLearnedSignal lastLearnedIr;
volatile bool g_isIrTransmitting = false;
volatile uint32_t g_lastIrTxEndTime = 0;
IrPresetButton g_irPresets[6] = {
  { "TV POWER", 0x20DF10EF, 0, 700, 0x8800 },
  { "AC POWER", 0x08800909, 0, 700, 0x03EF },
  { "VOL -",    0x20DFC03F, 0, 700, 0x18E3 },
  { "VOL +",    0x20DF40BF, 0, 700, 0x18E3 },
  { "CH -",     0x20DF807F, 0, 700, 0x18E3 },
  { "CH +",     0x20DF00FF, 0, 700, 0x18E3 }
};

AudioRecorderState audioState;
bool g_isScreenOn = true;
TouchPoint currentTouch;
ScreenPage currentPage = PAGE_HOME;
uint32_t currentEbookPage = 0;
uint32_t currentEbookIndex = 0;
uint32_t currentWikiArticle = 0;
uint32_t currentWikiPage = 0;

WiFiServer ftpServer(FTP_PORT);
WiFiServer ftpDataServer(FTP_DATA_PORT);
WiFiClient ftpClient;
String ftpCurrentDir = "/";
bool ftpAuthenticated = false;

AccessLogEntry accessLogs[MAX_ACCESS_LOGS];
size_t accessLogHead = 0;
size_t accessLogCount = 0;

#if CONFIG_IDF_TARGET_ESP32S3
USBMSC mscDrive;
#endif

// ---------------------------------------------------------------------------
// 開機初始化 (setup)
// ---------------------------------------------------------------------------
void setup() {
  setCpuFrequencyMhz(240); // 採用 240MHz 滿血雙核心時脈
  esp_log_level_set("*", ESP_LOG_NONE);
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial0.begin(115200);
#endif
  turnOffOnboardLeds(); // 關閉板載 LED，避免干擾螢幕重置腳位
  yield();

  logLine("\n==============================================");
  logLine("  ESP32-S3 SD HUB V3.5 工業級離線資料中心");
  logLine("==============================================");

  // 初始化遞迴 SPI 互斥鎖
  initSpiMutex();

  // 0. 初始化 2.8 吋 TFT 螢幕並顯示開機載入畫面
  initTftPinsEarly();
  initTftDisplay();
  showBootLoadingScreen(1, 8, "正在檢測 SD 記憶卡...");

  // 1. 掛載記憶卡 (支援自動降頻容錯: 40MHz -> 4MHz)
  bool sdOk = initSDCard();
  logf("[1/8] 記憶卡檢測…… %s\n", sdOk ? "✅ 掛載成功" : "❌ 未偵測到記憶卡");

  // 1.5 載入並雙向同步系統設定 (NVS Flash <-> SD 卡 /config.json)
  initRuntimeConfig();

  // 2. 啟動 Wi-Fi (AP + STA 雙模)
  showBootLoadingScreen(2, 8, "啟動 Wi-Fi 雙模網路與熱點...");
  startWiFi();

  // 3. 啟動 USB 隨身碟 MSC 模式
  showBootLoadingScreen(3, 8, "啟動 USB 隨身碟雙模讀取...");
#if CONFIG_IDF_TARGET_ESP32S3
  if (sdOk) initUSBMSCDrive();
#endif

  // 4. 啟動 Web 伺服器、FTP 伺服器與 ESP-NOW Mesh
  showBootLoadingScreen(4, 8, "啟動 Web 與 FTP 伺服器...");
  initWebServer();
  initFtpServer();
  initEspNowMesh();

  // 5. 啟動紅外線萬用遙控中心
  showBootLoadingScreen(5, 8, "啟動萬用紅外線遙控中心...");
  initIrRemote();

  // 6. 啟動 I2S 數位高解析麥克風
  showBootLoadingScreen(6, 8, "啟動 I2S 數位高解析麥克風...");
  initI2SMicrophone();
  if (sdOk) {
    startAudioRecording(); // 開機即啟動全時無縫連續收音
  }

  // 7. 啟動實體按鍵模組情境系統
  showBootLoadingScreen(7, 8, "初始化實體按鈕情境系統...");
  initButtonManager();

  printWelcomeGuide();
  logLine("🎉 開機完成！系統已就緒。");

  // 8. 載入完成過渡至首頁
  finishBootDisplay();
}

// ---------------------------------------------------------------------------
// 主迴圈 (loop) - 純非同步狀態機，0 阻塞
// ---------------------------------------------------------------------------
void loop() {
  handleCaptivePortalDNS(); // 處理強制登入門戶 (Captive Portal DNS)
  server.handleClient();     // 處理 Web HTTP 請求
  handleFtpServer();         // 處理 FTP 指令與資料傳輸
  checkThermalGuard();       // 晶片溫度動態守護與降頻散熱
  watchWiFiStatus();         // Wi-Fi 連線監控與自動重連
  checkWeatherLogTimer();    // 微氣象站自動 CSV 歸檔
  processEspNowInbox();      // 處理 ESP-NOW 急難留言與跨跳廣播
  handleAudioRecorderLoop(); // I2S 音訊任務健康維持
  checkIrReceiver();         // 監聽紅外線學習訊號
  handleButtonLoop();        // 實體按鈕 (GPIO 16 / BOOT 0) 點擊判定
  handleTouchEvents();       // 處理 M070 觸控點擊事件
  handleDisplayLoop();       // 螢幕局部動態刷新 (VU 表/計時器/遙測)
  yield();                   // 讓出 CPU 時間片，確保極致網路傳輸吞吐量
}
