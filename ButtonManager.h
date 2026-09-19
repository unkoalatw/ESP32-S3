#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include "Config.h"
#include "IrRemoteManager.h"
#include "AudioRecorderManager.h"
#include "TftDisplayManager.h"

// ---------------------------------------------------------------------------
// 實體按鈕模組管理器 (C036 輕觸開關 GPIO 16 / 板載 BOOT 鍵 GPIO 0)
// 1. 【單擊 (Single Click)】：情境式動作 (首頁休眠喚醒/紅外線發送/錄音開關/翻頁/診斷刷新)
// 2. 【雙擊 (Double Click)】：秒回首頁 (PAGE_HOME)
// 3. 【長按 0.8 秒 (Long Press)】：循環切換 8 大螢幕頁面
// ---------------------------------------------------------------------------

static uint8_t g_extBtnBaseline = HIGH;
static uint8_t g_bootBtnBaseline = HIGH;

inline void initButtonManager() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(ONBOARD_BOOT_PIN, INPUT_PULLUP);

  delay(50);

  int extHigh = 0, bootHigh = 0;
  for (int i = 0; i < 10; i++) {
    if (digitalRead(BUTTON_PIN) == HIGH) extHigh++;
    if (digitalRead(ONBOARD_BOOT_PIN) == HIGH) bootHigh++;
    delay(2);
  }
  g_extBtnBaseline  = (extHigh >= 5) ? HIGH : LOW;
  g_bootBtnBaseline = (bootHigh >= 5) ? HIGH : LOW;

  logf("[8/8] 實體按鈕情境系統 (GPIO %d, BOOT 0)…… ✅ 啟動完成\n", BUTTON_PIN);
}

// 根據當前 2.8 吋螢幕所在頁面自動切換單擊功能 (Context-Aware Action)
inline void executeButtonSingleClick() {
  if (!g_isScreenOn) {
    toggleScreenPower();
    return;
  }

  switch (currentPage) {
    case PAGE_HOME:
      logLine("🔘 [實體按鍵:主頁面] 觸發：開關螢幕休眠");
      toggleScreenPower();
      break;

    case PAGE_IR_REMOTE:
      logLine("🔘 [實體按鍵:紅外線頁] 觸發：發射第 1 鍵訊號");
      sendNecIrCode(g_irPresets[0].code);
      if (g_irPresets[0].code2 > 0) {
        delay(g_irPresets[0].delayMs > 0 ? g_irPresets[0].delayMs : 700);
        sendNecIrCode(g_irPresets[0].code2);
      }
      break;

    case PAGE_VOICE_MEMO:
      if (audioState.isRecording) {
        logLine("🔘 [實體按鍵:錄音頁] 觸發：停止錄音");
        stopAudioRecording();
      } else {
        logLine("🔘 [實體按鍵:錄音頁] 觸發：啟動錄音");
        startAudioRecording();
      }
      renderScreenVoiceMemo();
      break;

    case PAGE_WIFI_QR:
      logLine("🔘 [實體按鍵:QR頁] 觸發：返回首頁");
      currentPage = PAGE_HOME;
      switchScreenPage(currentPage);
      break;

    case PAGE_WIKI:
      logLine("🔘 [實體按鍵:維基頁] 觸發：閱讀下一頁");
      currentWikiPage++;
      renderScreenWiki();
      break;

    case PAGE_EBOOK:
      logLine("🔘 [實體按鍵:書庫頁] 觸發：閱讀下一頁");
      currentEbookPage++;
      renderScreenEbook();
      break;

    case PAGE_SYSTEM_INFO:
      logLine("🔘 [實體按鍵:系統頁] 觸發：刷新診斷狀態");
      renderScreenSystemInfo();
      break;

    default:
      currentPage = PAGE_HOME;
      switchScreenPage(currentPage);
      break;
  }
}

// 雙擊 (Double Click)：秒回首頁 (PAGE_HOME)
inline void executeButtonDoubleClick() {
  if (!g_isScreenOn) {
    toggleScreenPower();
    return;
  }
  logLine("🔘 [實體按鍵] 雙擊觸發：秒回首頁 (PAGE_HOME)");
  currentPage = PAGE_HOME;
  switchScreenPage(currentPage);
}

// 長按 0.8 秒 (Long Press)：循環切換 8 大螢幕頁面
inline void executeButtonLongPress() {
  if (!g_isScreenOn) {
    toggleScreenPower();
    return;
  }

  const ScreenPage pageCycle[] = {
    PAGE_HOME,
    PAGE_MENU,
    PAGE_WIFI_QR,
    PAGE_IR_REMOTE,
    PAGE_VOICE_MEMO,
    PAGE_WIKI,
    PAGE_EBOOK,
    PAGE_SYSTEM_INFO
  };
  int totalPages = sizeof(pageCycle) / sizeof(pageCycle[0]);
  int nextIdx = 0;
  for (int i = 0; i < totalPages; i++) {
    if (pageCycle[i] == currentPage) {
      nextIdx = (i + 1) % totalPages;
      break;
    }
  }
  currentPage = pageCycle[nextIdx];
  logf("🔘 [實體按鍵] 長按觸發：切換至頁面 [%s]\n", getPageName(currentPage));
  switchScreenPage(currentPage);
}

// 實體按鍵非阻塞即時偵測迴圈
inline void handleButtonLoop() {
  static uint32_t pressStartTime = 0;
  static uint32_t lastReleaseTime = 0;
  static bool isPressed = false;
  static bool longPressFired = false;
  static uint8_t clickCount = 0;

  uint8_t extVal  = digitalRead(BUTTON_PIN);
  uint8_t bootVal = digitalRead(ONBOARD_BOOT_PIN);

  bool extPressed = (extVal != g_extBtnBaseline);
  bool bootPressed = (bootVal == LOW);
  bool currentPressed = extPressed || bootPressed;

  uint32_t now = millis();

  if (currentPressed && !isPressed) {
    isPressed = true;
    pressStartTime = now;
    longPressFired = false;
  } else if (currentPressed && isPressed) {
    uint32_t holdMs = now - pressStartTime;
    if (holdMs >= 800 && !longPressFired) {
      longPressFired = true;
      clickCount = 0;
      executeButtonLongPress(); // 長按 0.8s 觸發翻頁
    }
  } else if (!currentPressed && isPressed) {
    isPressed = false;
    uint32_t holdMs = now - pressStartTime;
    if (holdMs >= 30 && !longPressFired) {
      clickCount++;
      lastReleaseTime = now;
    }
  }

  // 多擊判定超時 (280ms 內無後續點擊即執行單擊 / 雙擊)
  if (clickCount > 0 && !isPressed && (now - lastReleaseTime > 280)) {
    if (clickCount == 1) {
      executeButtonSingleClick();
    } else if (clickCount >= 2) {
      executeButtonDoubleClick();
    }
    clickCount = 0;
  }
}

#endif // BUTTON_MANAGER_H
