#ifndef TFT_DISPLAY_MANAGER_H
#define TFT_DISPLAY_MANAGER_H

#include "Config.h"
#include "IrRemoteManager.h"
#include "AudioRecorderManager.h"
#include "QRCodeGenerator.h"
#include <SPI.h>
#include <vector>

// ---------------------------------------------------------------------------
// M070 2.8 吋 SPI TFT (240x320) 與 XPT2046 觸控螢幕驅動管理 (工業風 UI)
// ---------------------------------------------------------------------------
constexpr int TFT_WIDTH  = 240;
constexpr int TFT_HEIGHT = 320;

static const SPISettings TFT_SPI_SETTINGS(20000000, MSBFIRST, SPI_MODE0);   // Display 20MHz
static const SPISettings TOUCH_SPI_SETTINGS(1500000, MSBFIRST, SPI_MODE0); // Touch 1.5MHz

// 16-bit RGB565 色彩定義
constexpr uint16_t COLOR_BLACK     = 0x0000;
constexpr uint16_t COLOR_NAVY      = 0x0842;
constexpr uint16_t COLOR_DARKCYAN  = 0x03EF;
constexpr uint16_t COLOR_LIGHTGREY = 0xC618;
constexpr uint16_t COLOR_DARKGREY  = 0x18C3;
constexpr uint16_t COLOR_BLUE      = 0x001F;
constexpr uint16_t COLOR_GREEN     = 0x07E0;
constexpr uint16_t COLOR_CYAN      = 0x07FF;
constexpr uint16_t COLOR_RED       = 0xF800;
constexpr uint16_t COLOR_YELLOW    = 0xFFE0;
constexpr uint16_t COLOR_WHITE     = 0xFFFF;
constexpr uint16_t COLOR_ORANGE    = 0xFD20;
constexpr uint16_t COLOR_BG_DARK   = 0x0842;
constexpr uint16_t COLOR_CARD_BG   = 0x10A2;
constexpr uint16_t COLOR_CARD_BRD  = 0x2124;
constexpr uint16_t COLOR_ACCENT    = 0x04BF;

// 螢幕 8 大頁面狀態機
enum ScreenPage {
  PAGE_HOME = 0,
  PAGE_MENU,
  PAGE_WIFI_QR,
  PAGE_IR_REMOTE,
  PAGE_VOICE_MEMO,
  PAGE_WIKI,
  PAGE_EBOOK,
  PAGE_SYSTEM_INFO
};

inline const char* getPageName(ScreenPage page) {
  switch (page) {
    case PAGE_HOME:        return "HOME";
    case PAGE_MENU:        return "MENU";
    case PAGE_WIFI_QR:     return "WIFI_QR";
    case PAGE_IR_REMOTE:   return "IR_REMOTE";
    case PAGE_VOICE_MEMO:  return "VOICE_MEMO";
    case PAGE_WIKI:        return "WIKI";
    case PAGE_EBOOK:       return "EBOOK";
    case PAGE_SYSTEM_INFO: return "SYSTEM_INFO";
    default:               return "UNKNOWN";
  }
}

constexpr uint32_t MAX_WIKI_ARTICLES = 50;
constexpr uint32_t MAX_EBOOK_PAGES    = 500;

struct TouchPoint {
  uint16_t x = 0;
  uint16_t y = 0;
  bool isPressed = false;
  bool justPressed = false;
  bool justReleased = false;
};

extern TouchPoint currentTouch;
extern ScreenPage currentPage;
extern bool g_isScreenOn;

static uint16_t lastRawX = 0, lastRawY = 0, lastZ1 = 0;
static bool lastIrqState = false;

// 前置宣告
inline void renderScreenHome();
inline void renderScreenMenu();
inline void renderScreenWiFiQR();
inline void renderScreenIrRemote();
inline void renderScreenVoiceMemo();
inline void renderScreenWiki();
inline void renderScreenEbook();
inline void renderScreenSystemInfo();
inline void showBootLoadingScreen(int step, int totalSteps, const char *msg);
inline void finishBootDisplay();
inline void switchScreenPage(ScreenPage newPage);
inline void toggleScreenPower();

// ---------------------------------------------------------------------------
// 標準 5x7 ASCII 字型點陣 (ASCII 32 ~ 126)
// ---------------------------------------------------------------------------
static const uint8_t font5x7[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, // 32 ' '
  0x00, 0x00, 0x5F, 0x00, 0x00, // 33 '!'
  0x00, 0x07, 0x00, 0x07, 0x00, // 34 '"'
  0x14, 0x7F, 0x14, 0x7F, 0x14, // 35 '#'
  0x24, 0x2A, 0x7F, 0x2A, 0x12, // 36 '$'
  0x23, 0x13, 0x08, 0x64, 0x62, // 37 '%'
  0x36, 0x49, 0x55, 0x22, 0x50, // 38 '&'
  0x00, 0x05, 0x03, 0x00, 0x00, // 39 '''
  0x00, 0x1C, 0x22, 0x41, 0x00, // 40 '('
  0x00, 0x41, 0x22, 0x1C, 0x00, // 41 ')'
  0x14, 0x08, 0x3E, 0x08, 0x14, // 42 '*'
  0x08, 0x08, 0x3E, 0x08, 0x08, // 43 '+'
  0x00, 0x50, 0x30, 0x00, 0x00, // 44 ','
  0x08, 0x08, 0x08, 0x08, 0x08, // 45 '-'
  0x00, 0x60, 0x60, 0x00, 0x00, // 46 '.'
  0x20, 0x10, 0x08, 0x04, 0x02, // 47 '/'
  0x3E, 0x51, 0x49, 0x45, 0x3E, // 48 '0'
  0x00, 0x42, 0x7F, 0x40, 0x00, // 49 '1'
  0x42, 0x61, 0x51, 0x49, 0x46, // 50 '2'
  0x21, 0x41, 0x45, 0x4B, 0x31, // 51 '3'
  0x18, 0x14, 0x12, 0x7F, 0x10, // 52 '4'
  0x27, 0x45, 0x45, 0x45, 0x39, // 53 '5'
  0x3C, 0x4A, 0x49, 0x49, 0x30, // 54 '6'
  0x01, 0x71, 0x09, 0x05, 0x03, // 55 '7'
  0x36, 0x49, 0x49, 0x49, 0x36, // 56 '8'
  0x06, 0x49, 0x49, 0x29, 0x1E, // 57 '9'
  0x00, 0x36, 0x36, 0x00, 0x00, // 58 ':'
  0x00, 0x56, 0x36, 0x00, 0x00, // 59 ';'
  0x08, 0x14, 0x22, 0x41, 0x00, // 60 '<'
  0x14, 0x14, 0x14, 0x14, 0x14, // 61 '='
  0x00, 0x41, 0x22, 0x14, 0x08, // 62 '>'
  0x02, 0x01, 0x51, 0x09, 0x06, // 63 '?'
  0x32, 0x49, 0x79, 0x41, 0x3E, // 64 '@'
  0x7E, 0x11, 0x11, 0x11, 0x7E, // 65 'A'
  0x7F, 0x49, 0x49, 0x49, 0x36, // 66 'B'
  0x3E, 0x41, 0x41, 0x41, 0x22, // 67 'C'
  0x7F, 0x41, 0x41, 0x22, 0x1C, // 68 'D'
  0x7F, 0x49, 0x49, 0x49, 0x41, // 69 'E'
  0x7F, 0x09, 0x09, 0x09, 0x01, // 70 'F'
  0x3E, 0x41, 0x49, 0x49, 0x7A, // 71 'G'
  0x7F, 0x08, 0x08, 0x08, 0x7F, // 72 'H'
  0x00, 0x41, 0x7F, 0x41, 0x00, // 73 'I'
  0x20, 0x40, 0x41, 0x3F, 0x01, // 74 'J'
  0x7F, 0x08, 0x14, 0x22, 0x41, // 75 'K'
  0x7F, 0x40, 0x40, 0x40, 0x40, // 76 'L'
  0x7F, 0x02, 0x0C, 0x02, 0x7F, // 77 'M'
  0x7F, 0x04, 0x08, 0x10, 0x7F, // 78 'N'
  0x3E, 0x41, 0x41, 0x41, 0x3E, // 79 'O'
  0x7F, 0x09, 0x09, 0x09, 0x06, // 80 'P'
  0x3E, 0x41, 0x51, 0x21, 0x5E, // 81 'Q'
  0x7F, 0x09, 0x19, 0x29, 0x46, // 82 'R'
  0x46, 0x49, 0x49, 0x49, 0x31, // 83 'S'
  0x01, 0x01, 0x7F, 0x01, 0x01, // 84 'T'
  0x3F, 0x40, 0x40, 0x40, 0x3F, // 85 'U'
  0x1F, 0x20, 0x40, 0x20, 0x1F, // 86 'V'
  0x3F, 0x40, 0x38, 0x40, 0x3F, // 87 'W'
  0x63, 0x14, 0x08, 0x14, 0x63, // 88 'X'
  0x07, 0x08, 0x70, 0x08, 0x07, // 89 'Y'
  0x61, 0x51, 0x49, 0x45, 0x43, // 90 'Z'
  0x00, 0x7F, 0x41, 0x41, 0x00, // 91 '['
  0x02, 0x04, 0x08, 0x10, 0x20, // 92 '\\'
  0x00, 0x41, 0x41, 0x7F, 0x00, // 93 ']'
  0x04, 0x02, 0x01, 0x02, 0x04, // 94 '^'
  0x40, 0x40, 0x40, 0x40, 0x40, // 95 '_'
  0x00, 0x01, 0x02, 0x04, 0x00, // 96 '`'
  0x20, 0x54, 0x54, 0x54, 0x78, // 97 'a'
  0x7F, 0x48, 0x44, 0x44, 0x38, // 98 'b'
  0x38, 0x44, 0x44, 0x44, 0x20, // 99 'c'
  0x38, 0x44, 0x44, 0x48, 0x7F, // 100 'd'
  0x38, 0x54, 0x54, 0x54, 0x18, // 101 'e'
  0x08, 0x7E, 0x09, 0x01, 0x02, // 102 'f'
  0x0C, 0x52, 0x52, 0x52, 0x3E, // 103 'g'
  0x7F, 0x08, 0x04, 0x04, 0x78, // 104 'h'
  0x00, 0x44, 0x7D, 0x40, 0x00, // 105 'i'
  0x20, 0x40, 0x44, 0x3D, 0x00, // 106 'j'
  0x7F, 0x10, 0x28, 0x44, 0x00, // 107 'k'
  0x00, 0x41, 0x7F, 0x40, 0x00, // 108 'l'
  0x7C, 0x04, 0x18, 0x04, 0x78, // 109 'm'
  0x7C, 0x08, 0x04, 0x04, 0x78, // 110 'n'
  0x38, 0x44, 0x44, 0x44, 0x38, // 111 'o'
  0x7C, 0x14, 0x14, 0x14, 0x08, // 112 'p'
  0x08, 0x14, 0x14, 0x18, 0x7C, // 113 'q'
  0x7C, 0x08, 0x04, 0x04, 0x08, // 114 'r'
  0x48, 0x54, 0x54, 0x54, 0x20, // 115 's'
  0x04, 0x3F, 0x44, 0x40, 0x20, // 116 't'
  0x3C, 0x40, 0x40, 0x20, 0x7C, // 117 'u'
  0x1C, 0x20, 0x40, 0x20, 0x1C, // 118 'v'
  0x3C, 0x40, 0x30, 0x40, 0x3C, // 119 'w'
  0x44, 0x28, 0x10, 0x28, 0x44, // 120 'x'
  0x0C, 0x50, 0x50, 0x50, 0x3C, // 121 'y'
  0x44, 0x64, 0x54, 0x4C, 0x44, // 122 'z'
  0x00, 0x08, 0x36, 0x41, 0x00, // 123 '{'
  0x00, 0x00, 0x7F, 0x00, 0x00, // 124 '|'
  0x00, 0x41, 0x36, 0x08, 0x00, // 125 '}'
  0x08, 0x08, 0x2A, 0x1C, 0x08  // 126 '~'
};

// ---------------------------------------------------------------------------
// 早期 SPI 引腳隔離 (防止 SD / Display / Touch 總線爭奪)
// ---------------------------------------------------------------------------
inline void initTftPinsEarly() {
  pinMode(LCD_CS, OUTPUT);
  pinMode(TOUCH_CS, OUTPUT);
  pinMode(SD_CS, OUTPUT);
  pinMode(LCD_DC, OUTPUT);
  pinMode(LCD_RST, OUTPUT);
  pinMode(TOUCH_IRQ, INPUT_PULLUP);

  if (LCD_BL >= 0) {
    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, HIGH);
  }

  digitalWrite(LCD_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  digitalWrite(LCD_RST, HIGH);

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, -1);
}

// ---------------------------------------------------------------------------
// 底層 SPI 繪圖原子操作
// ---------------------------------------------------------------------------
inline void tftWriteCommand(uint8_t cmd) {
  SpiLock lock;
  digitalWrite(TOUCH_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  SPI.beginTransaction(TFT_SPI_SETTINGS);
  digitalWrite(LCD_DC, LOW);
  digitalWrite(LCD_CS, LOW);
  SPI.transfer(cmd);
  digitalWrite(LCD_CS, HIGH);
  SPI.endTransaction();
}

inline void tftWriteData(uint8_t data) {
  SpiLock lock;
  digitalWrite(TOUCH_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  SPI.beginTransaction(TFT_SPI_SETTINGS);
  digitalWrite(LCD_DC, HIGH);
  digitalWrite(LCD_CS, LOW);
  SPI.transfer(data);
  digitalWrite(LCD_CS, HIGH);
  SPI.endTransaction();
}

inline void tftSetAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  tftWriteCommand(0x2A); // CASET
  tftWriteData(x0 >> 8);
  tftWriteData(x0 & 0xFF);
  tftWriteData(x1 >> 8);
  tftWriteData(x1 & 0xFF);

  tftWriteCommand(0x2B); // PASET
  tftWriteData(y0 >> 8);
  tftWriteData(y0 & 0xFF);
  tftWriteData(y1 >> 8);
  tftWriteData(y1 & 0xFF);

  tftWriteCommand(0x2C); // RAMWR
}

inline void tftFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (w <= 0 || h <= 0 || x >= TFT_WIDTH || y >= TFT_HEIGHT) return;
  if (x + w > TFT_WIDTH)  w = TFT_WIDTH - x;
  if (y + h > TFT_HEIGHT) h = TFT_HEIGHT - y;

  SpiLock lock;
  tftSetAddrWindow(static_cast<uint16_t>(x), static_cast<uint16_t>(y),
                   static_cast<uint16_t>(x + w - 1), static_cast<uint16_t>(y + h - 1));

  digitalWrite(TOUCH_CS, HIGH);
  digitalWrite(SD_CS, HIGH);
  SPI.beginTransaction(TFT_SPI_SETTINGS);
  digitalWrite(LCD_DC, HIGH);
  digitalWrite(LCD_CS, LOW);

  uint32_t totalPixels = static_cast<uint32_t>(w) * h;
  uint8_t hi = color >> 8;
  uint8_t lo = color & 0xFF;

  for (uint32_t i = 0; i < totalPixels; ++i) {
    SPI.transfer(hi);
    SPI.transfer(lo);
  }
  digitalWrite(LCD_CS, HIGH);
  SPI.endTransaction();
}

inline void tftDrawPixel(int16_t x, int16_t y, uint16_t color) {
  if (x < 0 || x >= TFT_WIDTH || y < 0 || y >= TFT_HEIGHT) return;
  tftFillRect(x, y, 1, 1, color);
}

inline void tftClearScreen(uint16_t color = COLOR_BG_DARK) {
  tftFillRect(0, 0, TFT_WIDTH, TFT_HEIGHT, color);
}

inline void tftDrawChar(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t size = 1) {
  if (c < 32 || c > 126) c = ' ';
  uint16_t charIdx = static_cast<uint8_t>(c - 32) * 5;
  uint16_t w = 6 * size;
  uint16_t h = 8 * size;

  if (x + static_cast<int16_t>(w) <= 0 || x >= TFT_WIDTH || y + static_cast<int16_t>(h) <= 0 || y >= TFT_HEIGHT) {
    return;
  }

  SpiLock lock;
  if (x >= 0 && x + static_cast<int16_t>(w) <= TFT_WIDTH && y >= 0 && y + static_cast<int16_t>(h) <= TFT_HEIGHT) {
    tftSetAddrWindow(static_cast<uint16_t>(x), static_cast<uint16_t>(y),
                     static_cast<uint16_t>(x + w - 1), static_cast<uint16_t>(y + h - 1));
    digitalWrite(TOUCH_CS, HIGH);
    digitalWrite(SD_CS, HIGH);
    SPI.beginTransaction(TFT_SPI_SETTINGS);
    digitalWrite(LCD_DC, HIGH);
    digitalWrite(LCD_CS, LOW);

    uint8_t colorHi = color >> 8, colorLo = color & 0xFF;
    uint8_t bgHi    = bg >> 8,    bgLo    = bg & 0xFF;

    for (uint8_t r = 0; r < 8; ++r) {
      for (uint8_t sy = 0; sy < size; ++sy) {
        for (uint8_t col = 0; col < 6; ++col) {
          bool pixelSet = false;
          if (col < 5 && r < 7) {
            uint8_t line = pgm_read_byte(&font5x7[charIdx + col]);
            pixelSet = ((line >> r) & 1) != 0;
          }
          uint8_t hi = pixelSet ? colorHi : bgHi;
          uint8_t lo = pixelSet ? colorLo : bgLo;
          for (uint8_t sx = 0; sx < size; ++sx) {
            SPI.transfer(hi);
            SPI.transfer(lo);
          }
        }
      }
    }
    digitalWrite(LCD_CS, HIGH);
    SPI.endTransaction();
  } else {
    for (int col = 0; col < 5; ++col) {
      uint8_t line = pgm_read_byte(&font5x7[charIdx + col]);
      for (int r = 0; r < 7; ++r) {
        if ((line >> r) & 1) {
          if (size == 1) tftDrawPixel(x + col, y + r, color);
          else tftFillRect(x + col * size, y + r * size, size, size, color);
        } else if (bg != color) {
          if (size == 1) tftDrawPixel(x + col, y + r, bg);
          else tftFillRect(x + col * size, y + r * size, size, size, bg);
        }
      }
    }
  }
}

inline void tftPrint(int16_t x, int16_t y, const String &text, uint16_t color, uint16_t bg, uint8_t size = 1) {
  int16_t cursorX = x;
  int16_t cursorY = y;
  uint16_t charW  = 6 * size;
  uint16_t charH  = 8 * size;

  for (size_t i = 0; i < text.length(); ++i) {
    char c = text[i];
    if (c == '\n') {
      cursorY += charH + 2;
      cursorX = x;
    } else {
      tftDrawChar(cursorX, cursorY, c, color, bg, size);
      cursorX += charW;
      if (cursorX + charW > TFT_WIDTH) {
        cursorX = x;
        cursorY += charH + 2;
      }
    }
  }
}

// ---------------------------------------------------------------------------
// 頁面頂欄與底部導航列渲染
// ---------------------------------------------------------------------------
inline void renderPageHeader(const char *title, bool showBack = false) {
  tftFillRect(0, 0, TFT_WIDTH, 30, COLOR_NAVY);
  if (showBack) {
    tftFillRect(4, 4, 60, 22, COLOR_CARD_BG);
    tftPrint(10, 10, "< BACK", COLOR_YELLOW, COLOR_CARD_BG, 1);
    tftPrint(72, 10, title, COLOR_WHITE, COLOR_NAVY, 1);
  } else {
    tftPrint(10, 10, title, COLOR_WHITE, COLOR_NAVY, 1);
  }
  tftFillRect(0, 29, TFT_WIDTH, 1, COLOR_CYAN);
}

inline void renderBottomNavBar() {
  tftFillRect(0, 288, TFT_WIDTH, 32, COLOR_NAVY);
  tftFillRect(0, 287, TFT_WIDTH, 1, COLOR_CARD_BRD);

  uint16_t cHome = (currentPage == PAGE_HOME) ? COLOR_ACCENT : COLOR_NAVY;
  uint16_t cMenu = (currentPage == PAGE_MENU) ? COLOR_ACCENT : COLOR_NAVY;
  uint16_t cIr   = (currentPage == PAGE_IR_REMOTE) ? COLOR_ACCENT : COLOR_NAVY;
  uint16_t cRec  = (currentPage == PAGE_VOICE_MEMO) ? COLOR_ACCENT : COLOR_NAVY;

  tftFillRect(2, 290, 56, 28, cHome);
  tftPrint(14, 299, "HOME", (currentPage == PAGE_HOME) ? COLOR_WHITE : COLOR_LIGHTGREY, cHome, 1);

  tftFillRect(62, 290, 56, 28, cMenu);
  tftPrint(74, 299, "MENU", (currentPage == PAGE_MENU) ? COLOR_WHITE : COLOR_LIGHTGREY, cMenu, 1);

  tftFillRect(122, 290, 56, 28, cIr);
  tftPrint(134, 299, "IR", (currentPage == PAGE_IR_REMOTE) ? COLOR_WHITE : COLOR_LIGHTGREY, cIr, 1);

  tftFillRect(182, 290, 56, 28, cRec);
  tftPrint(194, 299, "REC", (currentPage == PAGE_VOICE_MEMO) ? COLOR_WHITE : COLOR_LIGHTGREY, cRec, 1);
}

// ---------------------------------------------------------------------------
// 1. PAGE_HOME (儀表板首頁)
// ---------------------------------------------------------------------------
inline void renderScreenHome() {
  tftClearScreen(COLOR_BG_DARK);
  renderPageHeader("ESP32-S3 SD HUB", false);

  // 狀態資訊卡片
  tftFillRect(8, 36, 224, 134, COLOR_CARD_BG);
  tftPrint(14, 44, "SYSTEM STATUS", COLOR_YELLOW, COLOR_CARD_BG, 1);

  tftPrint(14, 62, "AP SSID : " + runtimeApSsid.substring(0, 16), COLOR_WHITE, COLOR_CARD_BG, 1);
  tftPrint(14, 78, "AP PASS : " + runtimeApPassword.substring(0, 16), COLOR_LIGHTGREY, COLOR_CARD_BG, 1);
  tftPrint(14, 94, "AP IP   : 192.168.4.1", COLOR_CYAN, COLOR_CARD_BG, 1);

  String staIp = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "Disconnected";
  tftPrint(14, 110, "STA IP  : " + staIp, (WiFi.status() == WL_CONNECTED) ? COLOR_GREEN : COLOR_LIGHTGREY, COLOR_CARD_BG, 1);

  bool sdOk = (SD.cardType() != CARD_NONE);
  tftPrint(14, 126, "SD CARD : " + String(sdOk ? "40MHz Mounted [OK]" : "No Card"), sdOk ? COLOR_GREEN : COLOR_RED, COLOR_CARD_BG, 1);

  float tempC = 42.0f;
#ifdef CONFIG_IDF_TARGET_ESP32S3
  tempC = temperatureRead();
#endif
  char tBuf[32];
  snprintf(tBuf, sizeof(tBuf), "CHIP    : 240MHz (%.1f C)", tempC);
  tftPrint(14, 142, tBuf, COLOR_LIGHTGREY, COLOR_CARD_BG, 1);

  // 4 大功能快捷按鈕
  tftFillRect(8, 180, 108, 42, COLOR_ACCENT);
  tftPrint(16, 196, "APP MENU", COLOR_WHITE, COLOR_ACCENT, 1);

  tftFillRect(124, 180, 108, 42, 0x10A4);
  tftPrint(136, 196, "SYS INFO", COLOR_CYAN, 0x10A4, 1);

  tftFillRect(8, 230, 108, 42, 0x2945);
  tftPrint(20, 246, "IR REMOTE", COLOR_ORANGE, 0x2945, 1);

  tftFillRect(124, 230, 108, 42, 0x4882);
  tftPrint(134, 246, "VOICE REC", COLOR_YELLOW, 0x4882, 1);

  renderBottomNavBar();
}

// ---------------------------------------------------------------------------
// 2. PAGE_MENU (6 模組九宮格選單)
// ---------------------------------------------------------------------------
inline void renderScreenMenu() {
  tftClearScreen(COLOR_BG_DARK);
  renderPageHeader("Application Menu", true);

  // Row 1
  tftFillRect(8, 40, 108, 66, COLOR_CARD_BG);
  tftPrint(16, 56, "[1] WIFI", COLOR_GREEN, COLOR_CARD_BG, 1);
  tftPrint(16, 76, "QR & Hotspot", COLOR_LIGHTGREY, COLOR_CARD_BG, 1);

  tftFillRect(124, 40, 108, 66, COLOR_CARD_BG);
  tftPrint(132, 56, "[2] IR REMOTE", COLOR_ORANGE, COLOR_CARD_BG, 1);
  tftPrint(132, 76, "TV & AC Ctrl", COLOR_LIGHTGREY, COLOR_CARD_BG, 1);

  // Row 2
  tftFillRect(8, 114, 108, 66, COLOR_CARD_BG);
  tftPrint(16, 130, "[3] VOICE REC", COLOR_RED, COLOR_CARD_BG, 1);
  tftPrint(16, 150, "I2S WAV Studio", COLOR_LIGHTGREY, COLOR_CARD_BG, 1);

  tftFillRect(124, 114, 108, 66, COLOR_CARD_BG);
  tftPrint(132, 130, "[4] WIKI DOCS", COLOR_CYAN, COLOR_CARD_BG, 1);
  tftPrint(132, 150, "Offline Library", COLOR_LIGHTGREY, COLOR_CARD_BG, 1);

  // Row 3
  tftFillRect(8, 188, 108, 66, COLOR_CARD_BG);
  tftPrint(16, 204, "[5] E-BOOK", COLOR_YELLOW, COLOR_CARD_BG, 1);
  tftPrint(16, 224, "Novel Reader", COLOR_LIGHTGREY, COLOR_CARD_BG, 1);

  tftFillRect(124, 188, 108, 66, COLOR_CARD_BG);
  tftPrint(132, 204, "[6] SYS INFO", COLOR_WHITE, COLOR_CARD_BG, 1);
  tftPrint(132, 224, "Diagnostics", COLOR_LIGHTGREY, COLOR_CARD_BG, 1);

  renderBottomNavBar();
}

// ---------------------------------------------------------------------------
// 3. PAGE_WIFI_QR (Wi-Fi QR Code 與連線憑證)
// ---------------------------------------------------------------------------
inline void renderScreenWiFiQR() {
  tftClearScreen(COLOR_BG_DARK);
  renderPageHeader("Wi-Fi Quick Join", true);

  String wifiQrPayload = "WIFI:S:" + runtimeApSsid + ";T:WPA;P:" + runtimeApPassword + ";;";
  QrCodeEngine::QRCode qr;
  QrCodeEngine::encode(wifiQrPayload, qr);

  int moduleScale = (qr.size <= 25) ? 4 : 3;
  int qrPxSize    = qr.size * moduleScale;
  int qrX         = (TFT_WIDTH - qrPxSize) / 2;
  int qrY         = 36;

  tftFillRect(qrX - 10, qrY - 10, qrPxSize + 20, qrPxSize + 20, COLOR_WHITE);

  for (int r = 0; r < qr.size; r++) {
    for (int c = 0; c < qr.size; c++) {
      if (QrCodeEngine::getModule(qr, c, r)) {
        tftFillRect(qrX + c * moduleScale, qrY + r * moduleScale, moduleScale, moduleScale, COLOR_BLACK);
      }
    }
  }

  int cardY = qrY + qrPxSize + 14;
  tftFillRect(8, cardY, 224, 268 - cardY, COLOR_CARD_BG);
  tftFillRect(8, cardY, 224, 1, COLOR_CARD_BRD);

  tftPrint(14, cardY + 6,  "📷 SCAN TO JOIN HOTSPOT", COLOR_YELLOW, COLOR_CARD_BG, 1);
  tftPrint(14, cardY + 22, "SSID : " + runtimeApSsid, COLOR_WHITE, COLOR_CARD_BG, 1);
  tftPrint(14, cardY + 38, "PASS : " + runtimeApPassword, COLOR_CYAN, COLOR_CARD_BG, 1);
  tftPrint(14, cardY + 54, "IP   : 192.168.4.1", COLOR_GREEN, COLOR_CARD_BG, 1);
  tftPrint(14, cardY + 70, "WEB  : http://sdserver.local", COLOR_LIGHTGREY, COLOR_CARD_BG, 1);
  tftPrint(14, cardY + 88, "[ Click Btn: Home | Long: Next ]", COLOR_YELLOW, COLOR_CARD_BG, 1);

  renderBottomNavBar();
}

// ---------------------------------------------------------------------------
// 4. PAGE_IR_REMOTE (紅外線萬用遙控器)
// ---------------------------------------------------------------------------
inline void renderScreenIrRemote() {
  tftClearScreen(COLOR_BG_DARK);
  renderPageHeader("IR Remote Controller", true);

  // Row 1: TV Power & AC Power
  tftFillRect(8, 40, 108, 46, 0x8800);
  tftPrint(20, 56, "TV POWER", COLOR_WHITE, 0x8800, 1);

  tftFillRect(124, 40, 108, 46, 0x03EF);
  tftPrint(136, 56, "AC POWER", COLOR_WHITE, 0x03EF, 1);

  // Row 2: Volume
  tftFillRect(8, 94, 108, 46, COLOR_CARD_BG);
  tftPrint(28, 110, "VOL -", COLOR_WHITE, COLOR_CARD_BG, 1);

  tftFillRect(124, 94, 108, 46, COLOR_CARD_BG);
  tftPrint(144, 110, "VOL +", COLOR_WHITE, COLOR_CARD_BG, 1);

  // Row 3: Channel
  tftFillRect(8, 148, 108, 46, COLOR_CARD_BG);
  tftPrint(32, 164, "CH -", COLOR_WHITE, COLOR_CARD_BG, 1);

  tftFillRect(124, 148, 108, 46, COLOR_CARD_BG);
  tftPrint(148, 164, "CH +", COLOR_WHITE, COLOR_CARD_BG, 1);

  // Row 4: Capture & Learn
  tftFillRect(8, 202, 224, 58, 0x1084);
  tftPrint(16, 212, "IR RECEIVER & LEARN (Pin 8):", COLOR_YELLOW, 0x1084, 1);
  if (lastLearnedIr.hasData) {
    char codeBuf[32];
    snprintf(codeBuf, sizeof(codeBuf), "CODE: 0x%08X (%s)", (unsigned)lastLearnedIr.code, lastLearnedIr.protocol.c_str());
    tftPrint(16, 232, codeBuf, COLOR_GREEN, 0x1084, 1);
  } else {
    tftPrint(16, 232, "Aim remote at pin 8...", COLOR_LIGHTGREY, 0x1084, 1);
  }

  renderBottomNavBar();
}

// ---------------------------------------------------------------------------
// 5. PAGE_VOICE_MEMO (I2S 錄音控制室)
// ---------------------------------------------------------------------------
inline void renderScreenVoiceMemo() {
  tftClearScreen(COLOR_BG_DARK);
  renderPageHeader("Voice Memo Studio", true);

  tftFillRect(8, 40, 224, 226, COLOR_CARD_BG);

  uint16_t btnColor = audioState.isRecording ? 0x8800 : COLOR_ACCENT;
  tftFillRect(20, 52, 200, 52, btnColor);
  if (audioState.isRecording) {
    tftPrint(48, 70, "[ STOP RECORDING ]", COLOR_WHITE, btnColor, 1);
  } else {
    tftPrint(44, 70, "[ START RECORDING ]", COLOR_WHITE, btnColor, 1);
  }

  tftPrint(20, 120, "MIC : INMP441 24-bit I2S (4,5,6)", COLOR_LIGHTGREY, COLOR_CARD_BG, 1);
  tftPrint(20, 138, "SD  : /recordings/*.wav", COLOR_CYAN, COLOR_CARD_BG, 1);

  char timeBuf[32];
  snprintf(timeBuf, sizeof(timeBuf), "TIMER: %02u:%02u", audioState.durationSec / 60, audioState.durationSec % 60);
  tftPrint(20, 160, timeBuf, audioState.isRecording ? COLOR_RED : COLOR_YELLOW, COLOR_CARD_BG, 1);

  tftPrint(20, 180, "LIVE AUDIO LEVEL:", COLOR_LIGHTGREY, COLOR_CARD_BG, 1);
  tftFillRect(20, 196, 200, 12, 0x0000);
  if (audioState.isRecording) {
    uint16_t vuWidth = 40 + (millis() % 140);
    tftFillRect(20, 196, vuWidth, 12, COLOR_GREEN);
  }

  tftPrint(20, 224, "Format: 16000Hz 16-bit Mono", COLOR_LIGHTGREY, COLOR_CARD_BG, 1);

  renderBottomNavBar();
}

// ---------------------------------------------------------------------------
// 6. PAGE_WIKI (離線 Markdown 文件庫，支援 SpiLock 與自動折行分頁)
// ---------------------------------------------------------------------------
inline void renderScreenWiki() {
  tftClearScreen(COLOR_BG_DARK);
  renderPageHeader("Offline Wiki / Docs", true);

  std::vector<String> wikiFiles;
  if (SD.cardType() != CARD_NONE) {
    ensureDirectoryExists("/wiki");
    SpiLock lock;
    File dir = SD.open("/wiki");
    if (dir && dir.isDirectory()) {
      File f = dir.openNextFile();
      while (f) {
        if (!f.isDirectory()) {
          String fn = leafName(String(f.name()));
          if (fn.endsWith(".md") || fn.endsWith(".txt")) {
            wikiFiles.push_back(fn);
          }
        }
        f = dir.openNextFile();
      }
      dir.close();
    }
  }

  if (wikiFiles.empty() && SD.cardType() != CARD_NONE) {
    SpiLock lock;
    File f1 = SD.open("/wiki/ESP32S3_Guide.md", FILE_WRITE);
    if (f1) {
      f1.println("# ESP32-S3 Overview");
      f1.println("Dual-core Xtensa LX7 SoC running up to 240MHz with vector AI engine.");
      f1.println("## Storage & RAM");
      f1.println("16MB SPI Flash + 8MB Octal PSRAM. 40MHz MicroSD SDIO/SPI.");
      f1.println("## Connectivity");
      f1.println("Wi-Fi AP/STA and ESP-NOW Mesh ready. 100% offline standalone.");
      f1.flush();
      f1.close();
      wikiFiles.push_back("ESP32S3_Guide.md");
    }
  }

  tftFillRect(8, 38, 224, 182, COLOR_CARD_BG);

  if (wikiFiles.empty()) {
    tftPrint(14, 60, "NO WIKI DOCS FOUND", COLOR_RED, COLOR_CARD_BG, 1);
    tftPrint(14, 84, "Please upload .md files", COLOR_WHITE, COLOR_CARD_BG, 1);
    tftPrint(14, 104, "to SD: /wiki/ folder", COLOR_CYAN, COLOR_CARD_BG, 1);
    tftPrint(14, 128, "via Web File Manager.", COLOR_LIGHTGREY, COLOR_CARD_BG, 1);
  } else {
    currentWikiArticle = currentWikiArticle % wikiFiles.size();
    String curFilename = wikiFiles[currentWikiArticle];
    String fullPath = "/wiki/" + curFilename;

    std::vector<String> formattedLines;
    std::vector<uint16_t> lineColors;

    {
      SpiLock lock;
      File wf = SD.open(fullPath, FILE_READ);
      if (wf) {
        while (wf.available()) {
          String rawLine = wf.readStringUntil('\n');
          rawLine.trim();
          if (rawLine.length() == 0) {
            formattedLines.push_back("");
            lineColors.push_back(COLOR_CARD_BG);
            continue;
          }

          uint16_t col = COLOR_WHITE;
          if (rawLine.startsWith("# ")) {
            col = COLOR_YELLOW;
            rawLine = rawLine.substring(2);
          } else if (rawLine.startsWith("## ")) {
            col = COLOR_CYAN;
            rawLine = rawLine.substring(3);
          } else if (rawLine.startsWith("### ")) {
            col = COLOR_GREEN;
            rawLine = rawLine.substring(4);
          } else if (rawLine.startsWith("- ") || rawLine.startsWith("* ")) {
            col = COLOR_LIGHTGREY;
            rawLine = " * " + rawLine.substring(2);
          }

          while (rawLine.length() > 0) {
            if (rawLine.length() <= 27) {
              formattedLines.push_back(rawLine);
              lineColors.push_back(col);
              break;
            } else {
              int splitPos = 27;
              int spacePos = rawLine.lastIndexOf(' ', 27);
              if (spacePos > 12) splitPos = spacePos;
              formattedLines.push_back(rawLine.substring(0, splitPos));
              lineColors.push_back(col);
              rawLine = rawLine.substring(splitPos);
              rawLine.trim();
            }
          }
        }
        wf.close();
      }
    }

    const size_t LINES_PER_PAGE = 8;
    size_t totalPages = (formattedLines.size() + LINES_PER_PAGE - 1) / LINES_PER_PAGE;
    if (totalPages == 0) totalPages = 1;
    if (currentWikiPage >= totalPages) currentWikiPage = 0;

    tftFillRect(8, 38, 224, 20, 0x10A4);
    char titleHdr[48];
    snprintf(titleHdr, sizeof(titleHdr), "[Doc %d/%d|P.%d/%d] %.10s",
             static_cast<int>(currentWikiArticle + 1), static_cast<int>(wikiFiles.size()),
             static_cast<int>(currentWikiPage + 1), static_cast<int>(totalPages), curFilename.c_str());
    tftPrint(12, 44, titleHdr, COLOR_YELLOW, 0x10A4, 1);

    size_t startLine = currentWikiPage * LINES_PER_PAGE;
    int y = 62;
    for (size_t i = 0; i < LINES_PER_PAGE && (startLine + i) < formattedLines.size(); i++) {
      String line = formattedLines[startLine + i];
      if (line.length() > 0) {
        tftPrint(14, y, line, lineColors[startLine + i], COLOR_CARD_BG, 1);
      }
      y += 18;
    }
  }

  tftFillRect(8, 224, 108, 26, COLOR_ACCENT);
  tftPrint(20, 232, "< PREV PAGE", COLOR_WHITE, COLOR_ACCENT, 1);

  tftFillRect(124, 224, 108, 26, COLOR_ACCENT);
  tftPrint(136, 232, "NEXT PAGE >", COLOR_WHITE, COLOR_ACCENT, 1);

  tftFillRect(8, 254, 224, 22, 0x2124);
  tftPrint(42, 260, "[ SWITCH NEXT DOC >> ]", COLOR_YELLOW, 0x2124, 1);

  renderBottomNavBar();
}

// ---------------------------------------------------------------------------
// 7. PAGE_EBOOK (離線電子書閱讀器)
// ---------------------------------------------------------------------------
inline void renderScreenEbook() {
  tftClearScreen(COLOR_BG_DARK);
  renderPageHeader("E-Book Reader", true);

  std::vector<String> bookFiles;
  if (SD.cardType() != CARD_NONE) {
    ensureDirectoryExists("/books");
    SpiLock lock;
    File dir = SD.open("/books");
    if (dir && dir.isDirectory()) {
      File f = dir.openNextFile();
      while (f) {
        if (!f.isDirectory()) {
          String fn = leafName(String(f.name()));
          if (fn.endsWith(".txt") || fn.endsWith(".md") || fn.endsWith(".epub")) {
            bookFiles.push_back(fn);
          }
        }
        f = dir.openNextFile();
      }
      dir.close();
    }
  }

  if (bookFiles.empty() && SD.cardType() != CARD_NONE) {
    SpiLock lock;
    File bf = SD.open("/books/sample_novel.txt", FILE_WRITE);
    if (bf) {
      bf.println("Chapter 1: The Frontier");
      bf.println("A standalone server hummed quietly.");
      bf.println("Gigabytes of knowledge stored offline.");
      bf.println("Reading across the 2.8 inch screen.");
      bf.println("Free from cellular boundaries.");
      bf.println("Empowering offline mobility.");
      bf.flush();
      bf.close();
      bookFiles.push_back("sample_novel.txt");
    }
  }

  tftFillRect(8, 38, 224, 182, 0x0000);

  if (bookFiles.empty()) {
    tftPrint(14, 60, "NO E-BOOKS FOUND", COLOR_RED, 0x0000, 1);
    tftPrint(14, 84, "Please upload .txt / .epub", COLOR_WHITE, 0x0000, 1);
    tftPrint(14, 104, "to SD: /books/ folder", COLOR_YELLOW, 0x0000, 1);
    tftPrint(14, 128, "via Web File Manager.", COLOR_LIGHTGREY, 0x0000, 1);
  } else {
    String curBookName = bookFiles[0];
    String fullPath = "/books/" + curBookName;

    tftFillRect(8, 38, 224, 20, 0x1084);

    SpiLock lock;
    File bf = SD.open(fullPath, FILE_READ);
    uint32_t totalBytes = bf ? bf.size() : 0;
    uint32_t bytesPerPage = 180;
    uint32_t maxPages = (totalBytes > 0) ? ((totalBytes + bytesPerPage - 1) / bytesPerPage) : 1;
    if (maxPages == 0) maxPages = 1;
    if (currentEbookPage >= maxPages) currentEbookPage = maxPages - 1;

    char titleHdr[48];
    snprintf(titleHdr, sizeof(titleHdr), "P.%lu/%lu: %.16s",
             (unsigned long)(currentEbookPage + 1), (unsigned long)maxPages, curBookName.c_str());
    tftPrint(12, 44, titleHdr, COLOR_YELLOW, 0x1084, 1);

    if (bf) {
      bf.seek(currentEbookPage * bytesPerPage);
      int y = 66;
      int lineCount = 0;
      while (bf.available() && lineCount < 7 && y < 210) {
        String line = bf.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
          if (line.length() > 27) line = line.substring(0, 27) + "...";
          tftPrint(14, y, line, COLOR_WHITE, 0x0000, 1);
          y += 18;
          lineCount++;
        }
      }
      bf.close();
    }
  }

  tftFillRect(8, 228, 108, 40, COLOR_ACCENT);
  tftPrint(24, 242, "< PREV PAGE", COLOR_WHITE, COLOR_ACCENT, 1);

  tftFillRect(124, 228, 108, 40, COLOR_ACCENT);
  tftPrint(140, 242, "NEXT PAGE >", COLOR_WHITE, COLOR_ACCENT, 1);

  renderBottomNavBar();
}

// ---------------------------------------------------------------------------
// 8. PAGE_SYSTEM_INFO (10 點硬體接線診斷與即時遙測)
// ---------------------------------------------------------------------------
struct WiringTestResult {
  bool lcdSpi;
  bool lcdCs;
  bool lcdDc;
  bool lcdRst;
  bool touchSpi;
  bool touchCs;
  bool touchIrq;
  bool sdCard;
  bool irReceiver;
  bool micPins;
};

static WiringTestResult lastWiringTest;

inline WiringTestResult runWiringDiagnostic() {
  WiringTestResult r;

  // TEST 1: ILI9341 Display SPI (Read Display ID via 0x04)
  {
    SpiLock lock;
    digitalWrite(TOUCH_CS, HIGH);
    digitalWrite(SD_CS, HIGH);

    SPI.beginTransaction(TFT_SPI_SETTINGS);
    digitalWrite(LCD_DC, LOW);
    digitalWrite(LCD_CS, LOW);
    SPI.transfer(0x04);
    digitalWrite(LCD_DC, HIGH);
    SPI.transfer(0x00);
    uint8_t id2 = SPI.transfer(0x00);
    uint8_t id3 = SPI.transfer(0x00);
    uint8_t id4 = SPI.transfer(0x00);
    digitalWrite(LCD_CS, HIGH);
    SPI.endTransaction();

    bool allZero = (id2 == 0 && id3 == 0 && id4 == 0);
    bool allFF   = (id2 == 0xFF && id3 == 0xFF && id4 == 0xFF);
    r.lcdSpi = (!allZero && !allFF);
  }

  // TEST 2..4: LCD Pins
  r.lcdCs  = (LCD_CS >= 0);
  r.lcdDc  = (LCD_DC >= 0);
  r.lcdRst = (LCD_RST >= 0);

  // TEST 5: XPT2046 Touch SPI Response
  {
    SpiLock lock;
    digitalWrite(LCD_CS, HIGH);
    digitalWrite(SD_CS, HIGH);

    SPI.beginTransaction(TOUCH_SPI_SETTINGS);
    digitalWrite(TOUCH_CS, LOW);
    delayMicroseconds(5);

    uint8_t buf[3] = { 0xD0, 0x00, 0x00 };
    SPI.transfer(buf, 3);

    uint8_t pd[3] = { 0xD0, 0x00, 0x00 };
    SPI.transfer(pd, 3);

    digitalWrite(TOUCH_CS, HIGH);
    SPI.endTransaction();

    bool rawAllFF = (buf[1] == 0xFF && buf[2] == 0xFF);
    r.touchSpi = !rawAllFF;
  }
  // 診斷測試後重新武裝觸控 PENIRQ
  rearmTouchHardware();

  // TEST 6..7: Touch CS and IRQ
  r.touchCs = (TOUCH_CS >= 0);
  {
    pinMode(TOUCH_IRQ, INPUT_PULLUP);
    delayMicroseconds(50);
    r.touchIrq = (digitalRead(TOUCH_IRQ) == HIGH);
  }

  // TEST 8: SD Card
  {
    SpiLock lock;
    r.sdCard = (SD.cardType() != CARD_NONE);
  }

  // TEST 9: IR RX (VS1838B idles HIGH)
  {
    pinMode(IR_RX_PIN, INPUT);
    delayMicroseconds(100);
    r.irReceiver = (digitalRead(IR_RX_PIN) == HIGH);
  }

  // TEST 10: I2S Microphone Pin validity
  {
    bool pinsUnique = (MIC_I2S_SD != MIC_I2S_SCK) && (MIC_I2S_SD != MIC_I2S_WS) && (MIC_I2S_SCK != MIC_I2S_WS);
    bool pinsValid  = (MIC_I2S_SD >= 0 && MIC_I2S_SD <= 48) && (MIC_I2S_SCK >= 0 && MIC_I2S_SCK <= 48) && (MIC_I2S_WS >= 0 && MIC_I2S_WS <= 48);
    r.micPins = pinsUnique && pinsValid;
  }

  lastWiringTest = r;
  return r;
}

inline void renderScreenSystemInfo() {
  tftClearScreen(COLOR_BLACK);
  renderPageHeader("FULL DIAGNOSTIC", true);

  WiringTestResult w = runWiringDiagnostic();

  float tempVal = 42.0f;
#ifdef CONFIG_IDF_TARGET_ESP32S3
  tempVal = temperatureRead();
#endif

  uint32_t freeHeap = ESP.getFreeHeap() / 1024;
  uint64_t sdTotal = 0, sdUsed = 0;
  if (w.sdCard) {
    SpiLock lock;
    sdTotal = SD.totalBytes() / (1024ULL * 1024);
    sdUsed  = SD.usedBytes() / (1024ULL * 1024);
  }

  char buf[48];
  int y = 34;

  // Section 1: System
  tftFillRect(0, y, 240, 10, COLOR_NAVY);
  tftPrint(4, y + 1, "-- SYSTEM --", COLOR_CYAN, COLOR_NAVY, 1);
  y += 12;

  snprintf(buf, sizeof(buf), "MCU:ESP32-S3 T:%.0fC H:%luK", tempVal, (unsigned long)freeHeap);
  tftPrint(2, y, buf, (tempVal < 80) ? COLOR_GREEN : COLOR_RED, COLOR_BLACK, 1);
  y += 11;

  if (w.sdCard) {
    snprintf(buf, sizeof(buf), "SD :%luMB/%luMB  WiFi:%s", (unsigned long)sdUsed, (unsigned long)sdTotal, WiFi.isConnected() ? "ON" : "OFF");
  } else {
    snprintf(buf, sizeof(buf), "SD :NOT MOUNTED   WiFi:%s", WiFi.isConnected() ? "ON" : "OFF");
  }
  tftPrint(2, y, buf, w.sdCard ? COLOR_GREEN : COLOR_RED, COLOR_BLACK, 1);
  y += 13;

  // Section 2: 10-Point Wiring Test
  tftFillRect(0, y, 240, 10, COLOR_NAVY);
  tftPrint(4, y + 1, "-- WIRING TEST --", COLOR_CYAN, COLOR_NAVY, 1);
  y += 12;

  int wy = y;
  snprintf(buf, sizeof(buf), "[%s]LCD SPI", w.lcdSpi ? "+" : "X");
  tftPrint(2, wy, buf, w.lcdSpi ? COLOR_GREEN : COLOR_RED, COLOR_BLACK, 1);
  snprintf(buf, sizeof(buf), "[%s]LCD CS:G9", w.lcdCs ? "+" : "X");
  tftPrint(122, wy, buf, w.lcdCs ? COLOR_GREEN : COLOR_RED, COLOR_BLACK, 1);
  wy += 11;

  snprintf(buf, sizeof(buf), "[%s]LCD DC:G14", w.lcdDc ? "+" : "X");
  tftPrint(2, wy, buf, w.lcdDc ? COLOR_GREEN : COLOR_RED, COLOR_BLACK, 1);
  snprintf(buf, sizeof(buf), "[%s]LCD RST:G21", w.lcdRst ? "+" : "X");
  tftPrint(122, wy, buf, w.lcdRst ? COLOR_GREEN : COLOR_RED, COLOR_BLACK, 1);
  wy += 11;

  snprintf(buf, sizeof(buf), "[%s]TCH SPI", w.touchSpi ? "+" : "X");
  tftPrint(2, wy, buf, w.touchSpi ? COLOR_GREEN : COLOR_RED, COLOR_BLACK, 1);
  snprintf(buf, sizeof(buf), "[%s]TCH CS:G7", w.touchCs ? "+" : "X");
  tftPrint(122, wy, buf, w.touchCs ? COLOR_GREEN : COLOR_RED, COLOR_BLACK, 1);
  wy += 11;

  snprintf(buf, sizeof(buf), "[%s]TCH IRQ:%s", w.touchIrq ? "+" : "X", w.touchIrq ? "OK" : "STUCK!");
  tftPrint(2, wy, buf, w.touchIrq ? COLOR_GREEN : COLOR_RED, COLOR_BLACK, 1);
  snprintf(buf, sizeof(buf), "[%s]SD CARD", w.sdCard ? "+" : "X");
  tftPrint(122, wy, buf, w.sdCard ? COLOR_GREEN : COLOR_RED, COLOR_BLACK, 1);
  wy += 11;

  snprintf(buf, sizeof(buf), "[%s]IR RX:G8", w.irReceiver ? "+" : "X");
  tftPrint(2, wy, buf, w.irReceiver ? COLOR_GREEN : COLOR_RED, COLOR_BLACK, 1);
  snprintf(buf, sizeof(buf), "[%s]MIC:G4/5/6", w.micPins ? "+" : "X");
  tftPrint(122, wy, buf, w.micPins ? COLOR_GREEN : COLOR_RED, COLOR_BLACK, 1);
  wy += 13;

  int passCount = (int)w.lcdSpi + w.lcdCs + w.lcdDc + w.lcdRst + w.touchSpi + w.touchCs + w.touchIrq + w.sdCard + w.irReceiver + w.micPins;
  snprintf(buf, sizeof(buf), "WIRING: %d/10 PASS", passCount);
  uint16_t sumColor = (passCount == 10) ? COLOR_GREEN : ((passCount >= 7) ? COLOR_YELLOW : COLOR_RED);
  tftPrint(2, wy, buf, sumColor, COLOR_BLACK, 1);
  y = wy + 13;

  // Section 3: Live Touch
  tftFillRect(0, y, 240, 10, COLOR_NAVY);
  tftPrint(4, y + 1, "-- TOUCH LIVE --", COLOR_CYAN, COLOR_NAVY, 1);
  y += 12;

  snprintf(buf, sizeof(buf), "RAW:(%04u,%04u) Z:%03u IRQ:%d", lastRawX, lastRawY, lastZ1, lastIrqState ? 1 : 0);
  tftPrint(2, y, buf, lastZ1 > 10 ? COLOR_GREEN : COLOR_YELLOW, COLOR_BLACK, 1);
  y += 11;

  snprintf(buf, sizeof(buf), "SCR:(%03u,%03u) %s", currentTouch.x, currentTouch.y, currentTouch.isPressed ? "PRESS" : "IDLE ");
  tftPrint(2, y, buf, currentTouch.isPressed ? COLOR_GREEN : COLOR_DARKGREY, COLOR_BLACK, 1);

  renderBottomNavBar();
}

// ---------------------------------------------------------------------------
// 觸控硬體驅動 (XPT2046 逐字節傳輸與 PENIRQ 重新武裝)
// ---------------------------------------------------------------------------
// XPT2046 單一 SPI 命令：發送控制字節，讀回 12-bit ADC 值
// 呼叫前 TOUCH_CS 必須已是 LOW、SPI transaction 必須已啟動
inline uint16_t xpt2046Read12(uint8_t cmd) {
  SPI.transfer(cmd);
  delayMicroseconds(1);  // Tconv: XPT2046 需要時間完成 ADC 轉換
  uint8_t msb = SPI.transfer(0x00);
  uint8_t lsb = SPI.transfer(0x00);
  return ((((uint16_t)msb << 8) | lsb) >> 3) & 0x0FFF;
}

// 完整的觸控硬體重新武裝程序（確保 PENIRQ 正確啟用）
// 必須在每次 TFT 或 SD SPI 操作後呼叫，讓 XPT2046 的 T_IRQ 腳位可正確拉低
inline void rearmTouchHardware() {
  SpiLock lock;
  digitalWrite(LCD_CS, HIGH);
  digitalWrite(SD_CS, HIGH);

  SPI.beginTransaction(TOUCH_SPI_SETTINGS);
  digitalWrite(TOUCH_CS, LOW);
  delayMicroseconds(2);

  // 發送 power-down 命令 (PD1=0, PD0=0)，啟用 PENIRQ 並關閉 ADC/REF
  // 0xD0 = Start(1) A2A1A0(101=X) MODE(0=12bit) SER/DFR(0=diff) PD1(0) PD0(0)
  SPI.transfer(0xD0);
  SPI.transfer(0x00);
  SPI.transfer(0x00);

  digitalWrite(TOUCH_CS, HIGH);
  SPI.endTransaction();
  delayMicroseconds(50);  // 讓 PENIRQ 線穩定拉高 (無觸控時)
}

inline void switchScreenPage(ScreenPage newPage) {
  currentPage = newPage;
  switch (currentPage) {
    case PAGE_HOME:        renderScreenHome(); break;
    case PAGE_MENU:        renderScreenMenu(); break;
    case PAGE_WIFI_QR:     renderScreenWiFiQR(); break;
    case PAGE_IR_REMOTE:   renderScreenIrRemote(); break;
    case PAGE_VOICE_MEMO:  renderScreenVoiceMemo(); break;
    case PAGE_WIKI:        renderScreenWiki(); break;
    case PAGE_EBOOK:       renderScreenEbook(); break;
    case PAGE_SYSTEM_INFO: renderScreenSystemInfo(); break;
  }
  rearmTouchHardware();
}

inline void renderTftScreen(ScreenPage page) {
  switchScreenPage(page);
}

inline void toggleScreenPower() {
  g_isScreenOn = !g_isScreenOn;
  if (!g_isScreenOn) {
    if (LCD_BL >= 0) {
      digitalWrite(LCD_BL, LOW);
    }
    tftClearScreen(COLOR_BLACK);
    logLine("📺 [2.8吋螢幕] 已進入休眠省電模式 (Display Sleep / Pitch Black)");
  } else {
    if (LCD_BL >= 0) {
      digitalWrite(LCD_BL, HIGH);
    }
    logLine("📺 [2.8吋螢幕] 已喚醒螢幕顯示 (Display Wakeup / Active)");
    switchScreenPage(currentPage);
  }
}

static bool requireFingerRelease = false;
static uint32_t fingerReleaseLockTime = 0;

inline bool checkTouchPressed() {
  bool irq = (digitalRead(TOUCH_IRQ) == LOW);
  lastIrqState = irq;

  SpiLock lock;
  digitalWrite(LCD_CS, HIGH);
  digitalWrite(SD_CS, HIGH);

  // 保持 TOUCH_CS LOW 貫穿整個讀取序列，避免 ADC 管線被中斷重置
  SPI.beginTransaction(TOUCH_SPI_SETTINGS);
  digitalWrite(TOUCH_CS, LOW);
  delayMicroseconds(2);

  // 多次取樣並取中位數以降低雜訊 (3 組採樣)
  uint16_t xSamples[3], ySamples[3], zSamples[3];
  for (int i = 0; i < 3; i++) {
    xSamples[i] = xpt2046Read12(0xD0);  // X 座標 (PD=00: power down, PENIRQ enabled)
    ySamples[i] = xpt2046Read12(0x90);  // Y 座標
    zSamples[i] = xpt2046Read12(0xB0);  // Z1 壓力
  }

  // 最終發送 power-down 命令，確保 PENIRQ 重新啟用
  xpt2046Read12(0xD0);

  digitalWrite(TOUCH_CS, HIGH);
  SPI.endTransaction();
  delayMicroseconds(50);  // 讓 PENIRQ 線穩定

  // 簡易中位數：排序後取中間值 (對 3 個樣本排序)
  auto median3 = [](uint16_t a, uint16_t b, uint16_t c) -> uint16_t {
    if (a > b) { uint16_t t = a; a = b; b = t; }
    if (b > c) { uint16_t t = b; b = c; c = t; }
    if (a > b) { uint16_t t = a; a = b; b = t; }
    return b;
  };

  uint16_t x1 = median3(xSamples[0], xSamples[1], xSamples[2]);
  uint16_t y1 = median3(ySamples[0], ySamples[1], ySamples[2]);
  uint16_t z1 = median3(zSamples[0], zSamples[1], zSamples[2]);

  lastRawX = x1;
  lastRawY = y1;
  lastZ1   = z1;

  bool inValidRange = (x1 >= 100 && x1 <= 3900 && y1 >= 100 && y1 <= 3900);
  // 嚴格判定：必須有 PENIRQ 中斷拉低 + Z1 壓力感應大於閾值 + ADC 數值落在有效螢幕區間
  // 徹底杜絕無觸碰時因 SPI 匯流排浮接或雜訊導致的幽靈亂點 (Phantom Touch)
  bool isTouched    = irq && (z1 > 40) && inValidRange;

  if (isTouched) {
    int32_t cx = map(static_cast<int32_t>(x1), 3650, 350, 0, TFT_WIDTH);
    int32_t cy = map(static_cast<int32_t>(y1), 3750, 350, 0, TFT_HEIGHT);

    currentTouch.x = constrain(cx, 0, TFT_WIDTH - 1);
    currentTouch.y = constrain(cy, 0, TFT_HEIGHT - 1);
    currentTouch.isPressed = true;
    return true;
  }

  currentTouch.isPressed = false;
  return false;
}

static uint32_t lastTouchPressMs = 0;

inline void handleTouchEvents() {
  if (!g_isScreenOn) return;
  bool isPressed = checkTouchPressed();

  // 必須等手指真正離開螢幕表面 (!isPressed) 才能解除鎖定，杜絕長按重複誤觸
  if (requireFingerRelease) {
    if (!isPressed) {
      requireFingerRelease = false;
    } else {
      return;
    }
  }

  if (!isPressed) return;

  uint32_t now = millis();
  if (now - lastTouchPressMs < 150) return; // 150ms 防抖
  lastTouchPressMs = now;

  uint16_t tx = currentTouch.x;
  uint16_t ty = currentTouch.y;

  bool matched = false;

  // 1. 頂部返回按鈕 (< BACK: x: 0..75, y: 0..36)
  if (ty <= 36 && tx <= 75 && currentPage != PAGE_HOME) {
    switchScreenPage(PAGE_HOME);
    requireFingerRelease = true;
    fingerReleaseLockTime = millis();
    return;
  }

  // 2. 底部導航列 (y: 282..320)
  if (ty >= 282) {
    if (tx < 60)       switchScreenPage(PAGE_HOME);
    else if (tx < 120) switchScreenPage(PAGE_MENU);
    else if (tx < 180) switchScreenPage(PAGE_IR_REMOTE);
    else               switchScreenPage(PAGE_VOICE_MEMO);

    requireFingerRelease = true;
    fingerReleaseLockTime = millis();
    return;
  }

  // 3. 頁面專屬按鈕分派
  switch (currentPage) {
    case PAGE_HOME:
      if (ty >= 176 && ty <= 225) {
        if (tx >= 4 && tx <= 118) {
          switchScreenPage(PAGE_MENU);
          matched = true;
        } else if (tx >= 122 && tx <= 236) {
          switchScreenPage(PAGE_SYSTEM_INFO);
          matched = true;
        }
      } else if (ty >= 226 && ty <= 275) {
        if (tx >= 4 && tx <= 118) {
          switchScreenPage(PAGE_IR_REMOTE);
          matched = true;
        } else if (tx >= 122 && tx <= 236) {
          switchScreenPage(PAGE_VOICE_MEMO);
          matched = true;
        }
      }
      break;

    case PAGE_MENU:
      if (ty >= 36 && ty <= 110) {
        if (tx >= 4 && tx <= 118) {
          switchScreenPage(PAGE_WIFI_QR);
          matched = true;
        } else if (tx >= 122 && tx <= 236) {
          switchScreenPage(PAGE_IR_REMOTE);
          matched = true;
        }
      } else if (ty >= 111 && ty <= 184) {
        if (tx >= 4 && tx <= 118) {
          switchScreenPage(PAGE_VOICE_MEMO);
          matched = true;
        } else if (tx >= 122 && tx <= 236) {
          switchScreenPage(PAGE_WIKI);
          matched = true;
        }
      } else if (ty >= 185 && ty <= 258) {
        if (tx >= 4 && tx <= 118) {
          switchScreenPage(PAGE_EBOOK);
          matched = true;
        } else if (tx >= 122 && tx <= 236) {
          switchScreenPage(PAGE_SYSTEM_INFO);
          matched = true;
        }
      }
      break;

    case PAGE_WIFI_QR:
      if (ty >= 195 && ty <= 245 && tx >= 12 && tx <= 228) {
        switchScreenPage(PAGE_HOME);
        matched = true;
      }
      break;

    case PAGE_IR_REMOTE:
      if (ty >= 36 && ty <= 90) {
        if (tx >= 4 && tx <= 118) {
          sendNecIrCode(0x20DF10EF);
          matched = true;
        } else if (tx >= 122 && tx <= 236) {
          sendNecIrCode(0x8800909);
          matched = true;
        }
      } else if (ty >= 91 && ty <= 144) {
        if (tx >= 4 && tx <= 118) {
          sendNecIrCode(0x20DFC03F);
          matched = true;
        } else if (tx >= 122 && tx <= 236) {
          sendNecIrCode(0x20DF40BF);
          matched = true;
        }
      } else if (ty >= 145 && ty <= 198) {
        if (tx >= 4 && tx <= 118) {
          sendNecIrCode(0x20DF807F);
          matched = true;
        } else if (tx >= 122 && tx <= 236) {
          sendNecIrCode(0x20DF00FF);
          matched = true;
        }
      } else if (ty >= 199 && ty <= 265 && tx >= 4 && tx <= 236) {
        checkIrReceiver();
        renderScreenIrRemote();
        matched = true;
      }
      break;

    case PAGE_VOICE_MEMO:
      if (ty >= 48 && ty <= 110 && tx >= 15 && tx <= 225) {
        if (!audioState.isRecording) {
          startAudioRecording();
        } else {
          stopAudioRecording();
        }
        renderScreenVoiceMemo();
        matched = true;
      }
      break;

    case PAGE_WIKI:
      if (ty >= 224 && ty <= 252) {
        if (tx >= 4 && tx <= 118) {
          if (currentWikiPage > 0) currentWikiPage--;
          renderScreenWiki();
          matched = true;
        } else if (tx >= 122 && tx <= 236) {
          currentWikiPage++;
          renderScreenWiki();
          matched = true;
        }
      } else if (ty >= 254 && ty <= 276 && tx >= 4 && tx <= 236) {
        currentWikiArticle++;
        currentWikiPage = 0;
        renderScreenWiki();
        matched = true;
      }
      break;

    case PAGE_EBOOK:
      if (ty >= 224 && ty <= 272) {
        if (tx >= 4 && tx <= 118) {
          if (currentEbookPage > 0) currentEbookPage--;
          renderScreenEbook();
          matched = true;
        } else if (tx >= 122 && tx <= 236) {
          if (currentEbookPage < MAX_EBOOK_PAGES - 1) currentEbookPage++;
          renderScreenEbook();
          matched = true;
        }
      }
      break;

    case PAGE_SYSTEM_INFO:
      break;

    default:
      break;
  }

  if (matched) {
    requireFingerRelease = true;
    fingerReleaseLockTime = millis();
  }
}

// ---------------------------------------------------------------------------
// 非阻塞動態 UI 局部刷新 (VU 表 / 計時器 / 遙測)
// ---------------------------------------------------------------------------
inline void handleDisplayLoop() {
  if (!g_isScreenOn) return;
  static uint32_t lastDisplayUpdateMs = 0;
  uint32_t now = millis();
  if (now - lastDisplayUpdateMs < 200) return;
  lastDisplayUpdateMs = now;

  bool didRedraw = false;

  if (currentPage == PAGE_SYSTEM_INFO) {
    char diagBuf[48];
    snprintf(diagBuf, sizeof(diagBuf), "RAW:(%04u,%04u) Z:%03u IRQ:%d", lastRawX, lastRawY, lastZ1, lastIrqState ? 1 : 0);
    tftFillRect(0, 209, 240, 11, COLOR_BLACK);
    tftPrint(2, 209, diagBuf, lastZ1 > 10 ? COLOR_GREEN : COLOR_YELLOW, COLOR_BLACK, 1);

    snprintf(diagBuf, sizeof(diagBuf), "SCR:(%03u,%03u) %s", currentTouch.x, currentTouch.y, currentTouch.isPressed ? "PRESS" : "IDLE ");
    tftFillRect(0, 220, 240, 11, COLOR_BLACK);
    tftPrint(2, 220, diagBuf, currentTouch.isPressed ? COLOR_GREEN : COLOR_DARKGREY, COLOR_BLACK, 1);
    didRedraw = true;
  }

  if (currentPage == PAGE_VOICE_MEMO && audioState.isRecording) {
    char timeBuf[32];
    snprintf(timeBuf, sizeof(timeBuf), "TIMER: %02u:%02u", audioState.durationSec / 60, audioState.durationSec % 60);
    tftPrint(20, 160, timeBuf, COLOR_RED, COLOR_CARD_BG, 1);

    uint16_t vuWidth = 30 + (millis() % 140);
    tftFillRect(20, 196, 200, 12, 0x0000);
    tftFillRect(20, 196, vuWidth, 12, COLOR_GREEN);
    didRedraw = true;
  }

  // 無論是否有局部重繪，都必須重新武裝觸控 PENIRQ
  // 因為本輪 loop 中的其他 SPI 操作 (Web/FTP/SD) 可能已改變匯流排狀態
  rearmTouchHardware();
}

// ---------------------------------------------------------------------------
// 初始化 M070 2.8 吋 SPI Display (ILI9341 MADCTL 0x48 Normal Portrait)
// ---------------------------------------------------------------------------
inline void initTftDisplay() {
  pinMode(LCD_CS, OUTPUT);
  pinMode(LCD_DC, OUTPUT);
  pinMode(LCD_RST, OUTPUT);
  pinMode(TOUCH_CS, OUTPUT);
  pinMode(TOUCH_IRQ, INPUT_PULLUP);

  if (LCD_BL >= 0) {
    pinMode(LCD_BL, OUTPUT);
    digitalWrite(LCD_BL, g_isScreenOn ? HIGH : LOW);
  }

  digitalWrite(LCD_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);
  digitalWrite(SD_CS, HIGH);

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, -1);

  // 硬體重置
  digitalWrite(LCD_RST, HIGH);
  delay(10);
  digitalWrite(LCD_RST, LOW);
  delay(20);
  digitalWrite(LCD_RST, HIGH);
  delay(120);

  // ILI9341 初始化序列
  tftWriteCommand(0x01); // Software Reset
  delay(100);

  tftWriteCommand(0xCB); // Power control A
  tftWriteData(0x39);
  tftWriteData(0x2C);
  tftWriteData(0x00);
  tftWriteData(0x34);
  tftWriteData(0x02);

  tftWriteCommand(0xCF); // Power control B
  tftWriteData(0x00);
  tftWriteData(0xC1);
  tftWriteData(0x30);

  tftWriteCommand(0xE8); // Driver timing control A
  tftWriteData(0x85);
  tftWriteData(0x00);
  tftWriteData(0x78);

  tftWriteCommand(0xEA); // Driver timing control B
  tftWriteData(0x00);
  tftWriteData(0x00);

  tftWriteCommand(0xED); // Power on sequence control
  tftWriteData(0x64);
  tftWriteData(0x03);
  tftWriteData(0x12);
  tftWriteData(0x81);

  tftWriteCommand(0xF7); // Pump ratio control
  tftWriteData(0x20);

  tftWriteCommand(0xC0); // Power Control 1
  tftWriteData(0x23);

  tftWriteCommand(0xC1); // Power Control 2
  tftWriteData(0x10);

  tftWriteCommand(0xC5); // VCOM Control 1
  tftWriteData(0x3E);
  tftWriteData(0x28);

  tftWriteCommand(0xC7); // VCOM Control 2
  tftWriteData(0x86);

  // 螢幕方向：標準直向 Portrait (MADCTL = 0x48)
  tftWriteCommand(0x36);
  tftWriteData(0x48);

  tftWriteCommand(0x3A); // 16-bit RGB565
  tftWriteData(0x55);

  tftWriteCommand(0xB1); // Frame Rate Control
  tftWriteData(0x00);
  tftWriteData(0x18);

  tftWriteCommand(0xB6); // Display Function Control
  tftWriteData(0x08);
  tftWriteData(0x82);
  tftWriteData(0x27);

  tftWriteCommand(0x11); // Sleep Out
  delay(120);

  tftWriteCommand(0x29); // Display ON
  delay(50);

  // 初始化 XPT2046 觸控 PENIRQ 中斷待機模式
  rearmTouchHardware();

  tftClearScreen(COLOR_BLACK);

  if (!g_isScreenOn) {
    if (LCD_BL >= 0) {
      digitalWrite(LCD_BL, LOW);
    }
    logLine("[4/5] 2.8吋螢幕開機預設休眠省電模式 (純黑待機)…… ✅ 等待按鈕喚醒");
  } else {
    if (LCD_BL >= 0) {
      digitalWrite(LCD_BL, HIGH);
    }
    switchScreenPage(PAGE_HOME);
  }
}

// ---------------------------------------------------------------------------
// 開機進度載入畫面
// ---------------------------------------------------------------------------
inline void showBootLoadingScreen(int step, int totalSteps, const char *msg) {
  SpiLock lock;
  const int barX = 20;
  const int barY = 200;
  const int barW = TFT_WIDTH - 40;
  const int barH = 14;

  if (step <= 1) {
    tftClearScreen(COLOR_BLACK);
    tftPrint(50, 60, "ESP32-S3", COLOR_CYAN, COLOR_BLACK, 2);
    tftPrint(30, 90, "File Server", COLOR_WHITE, COLOR_BLACK, 2);
    tftFillRect(barX - 1, barY - 1, barW + 2, barH + 2, COLOR_DARKGREY);
    tftFillRect(barX, barY, barW, barH, COLOR_BLACK);
  }

  tftFillRect(10, 230, TFT_WIDTH - 20, 24, COLOR_BLACK);
  tftPrint(10, 232, String("[") + String(step) + "/" + String(totalSteps) + "] " + String(msg),
           COLOR_LIGHTGREY, COLOR_BLACK, 1);

  int fillW = static_cast<int>(static_cast<long>(barW) * step / totalSteps);
  if (fillW > 0) {
    tftFillRect(barX, barY, fillW, barH, COLOR_CYAN);
  }
}

inline void finishBootDisplay() {
  showBootLoadingScreen(8, 8, "系統就緒！");
  delay(300);
  g_isScreenOn = true;
  if (LCD_BL >= 0) digitalWrite(LCD_BL, HIGH);

  // 必須透過 switchScreenPage 進入首頁，確保 rearmTouchHardware() 被呼叫
  // 如果直接呼叫 renderScreenHome()，觸控 PENIRQ 不會被重新武裝
  switchScreenPage(PAGE_HOME);

  logLine("📺 [2.8吋螢幕] 開機完成，畫面已就緒並進入首頁！");
}

#endif // TFT_DISPLAY_MANAGER_H
