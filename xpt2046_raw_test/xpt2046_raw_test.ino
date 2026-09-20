/*
  ===========================================================================
   XPT2046 Touch Screen Minimal Hardware Diagnostic Tool
   用最精簡的程式碼，不載入任何 GUI、Web、SD 或複雜邏輯，
   100% 驗證 XPT2046 硬體 SPI 接線與電阻觸控 Raw Data 是否正常。
  ===========================================================================
*/
#include <Arduino.h>
#include <SPI.h>

// ---------------------------------------------------------------------------
// 腳位對應表 (請務必確認這 5 條線都有連接到 ESP32-S3)
// ---------------------------------------------------------------------------
// TFT Touch Pin     ESP32-S3 Pin
// T_CLK            GPIO 12 (與 LCD SCK / SD SCK 並聯)
// T_DIN (MOSI)     GPIO 11 (與 LCD MOSI / SD MOSI 並聯)
// T_DO  (MISO)     GPIO 13 (與 LCD MISO / SD MISO 並聯)
// T_CS             GPIO 7  (獨立 CS 腳位)
// T_IRQ            GPIO 15 (中斷輸入，可選)
// ---------------------------------------------------------------------------
constexpr int T_CLK = 12;
constexpr int T_DIN = 11; // ESP32 MOSI -> Touch DIN
constexpr int T_DO  = 13; // Touch DO -> ESP32 MISO
constexpr int T_CS  = 7;
constexpr int T_IRQ = 15;

// 其他 SPI 裝置的 CS 腳位（必須拉 HIGH 禁能，避免干擾匯流排）
constexpr int LCD_CS = 9;
constexpr int SD_CS  = 10;

SPISettings touchSPI(1000000, MSBFIRST, SPI_MODE0); // 1MHz 穩定頻率

uint16_t readXPT2046(uint8_t cmd) {
  SPI.transfer(cmd);
  delayMicroseconds(2);
  uint8_t msb = SPI.transfer(0x00);
  uint8_t lsb = SPI.transfer(0x00);
  return ((((uint16_t)msb << 8) | lsb) >> 3) & 0x0FFF;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // 1. 先把其他裝置 CS 拉高，防止 LCD / SD 佔用匯流排
  pinMode(LCD_CS, OUTPUT);
  digitalWrite(LCD_CS, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  // 2. 初始化 Touch CS 與 IRQ
  pinMode(T_CS, OUTPUT);
  digitalWrite(T_CS, HIGH);
  pinMode(T_IRQ, INPUT_PULLUP);

  // 3. 啟動 SPI 匯流排
  SPI.begin(T_CLK, T_DO, T_DIN, -1);

  Serial.println("\n==============================================");
  Serial.println("  XPT2046 觸控硬體底層診斷工具 (RAW TEST)");
  Serial.println("==============================================");
  Serial.println("提示：XPT2046 為電阻式觸控，請用指甲或觸控筆適度施壓螢幕。");
  Serial.println("若未按壓時數值跳動或全 0 / 4095，請檢查 T_CLK/T_DIN/T_DO 接線。\n");
}

inline int16_t touchBestTwoAvg(int16_t a, int16_t b, int16_t c) {
  int16_t ab = abs(a - b);
  int16_t ac = abs(a - c);
  int16_t bc = abs(b - c);
  if (ab <= ac && ab <= bc) return (a + b) / 2;
  if (ac <= ab && ac <= bc) return (a + c) / 2;
  return (b + c) / 2;
}

void loop() {
  SPI.beginTransaction(touchSPI);
  digitalWrite(T_CS, LOW);
  delayMicroseconds(2);

  // ---- Pressure (Z1 / Z2) ----
  SPI.transfer(0xB1);
  int16_t z1 = SPI.transfer16(0xC1) >> 3;
  int16_t z2 = SPI.transfer16(0x91) >> 3;
  int32_t pressure = (int32_t)z1 + 4095 - (int32_t)z2;

  uint16_t rawX = 0;
  uint16_t rawY = 0;

  if (pressure >= 250) {
    SPI.transfer16(0x91);
    int16_t a0 = SPI.transfer16(0xD1) >> 3;
    int16_t b0 = SPI.transfer16(0x91) >> 3;
    int16_t a1 = SPI.transfer16(0xD1) >> 3;
    int16_t b1 = SPI.transfer16(0x91) >> 3;
    int16_t a2 = SPI.transfer16(0xD0) >> 3;
    int16_t b2 = SPI.transfer16(0x0000) >> 3;

    rawX = touchBestTwoAvg(a0, a1, a2);
    rawY = touchBestTwoAvg(b0, b1, b2);
  } else {
    SPI.transfer16(0xD0);
    SPI.transfer16(0x0000);
  }

  digitalWrite(T_CS, HIGH);
  SPI.endTransaction();

  int irqState = digitalRead(T_IRQ);
  bool isPressed = (pressure >= 250) && (rawX >= 100 && rawX <= 4000 && rawY >= 100 && rawY <= 4000);

  Serial.printf("IRQ=%d | Z1=%4d | Z2=%4d | Press=%5ld | RAW_X=%4u | RAW_Y=%4u | %s\n",
                irqState, z1, z2, pressure, rawX, rawY,
                isPressed ? "🔥 [TOUCH DETECTED!]" : "   (Idle)");

  delay(100);
}
