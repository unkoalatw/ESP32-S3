#ifndef WEATHER_STATION_H
#define WEATHER_STATION_H

#include "Config.h"
#include "SDCardManager.h"

extern uint32_t lastWeatherLogTime;
extern uint32_t weatherLogIntervalSec;
extern uint32_t totalWeatherLogCount;

inline void ensureWeatherDir() {
  ensureDirectoryExists("/weather");
}

inline String getDailyWeatherCsvPath() {
  ensureWeatherDir();
  uint32_t dayIndex = (millis() / 1000) / 86400 + 1;
  char buf[32];
  snprintf(buf, sizeof(buf), "/weather/day_%03u.csv", static_cast<unsigned>(dayIndex));
  return String(buf);
}

inline void appendWeatherRecordCsv() {
  String csvPath = getDailyWeatherCsvPath();
  SpiLock lock;
  bool isNewFile = !SD.exists(csvPath);

  File f = SD.open(csvPath, FILE_APPEND);
  if (f) {
    if (isNewFile) {
      f.println("TimestampSec,UptimeSec,TempC,CpuMHz,FreeHeapBytes,FreePsramBytes,WiFiStatus");
    }
    float temp = 0.0f;
#ifdef CONFIG_IDF_TARGET_ESP32S3
    temp = temperatureRead();
#endif
    uint32_t nowSec = millis() / 1000;
    f.printf("%u,%u,%.1f,%u,%u,%u,%s\n",
             nowSec, nowSec, temp, ESP.getCpuFreqMHz(),
             ESP.getFreeHeap(), ESP.getFreePsram(),
             (WiFi.status() == WL_CONNECTED ? "CONNECTED" : "AP_ONLY"));
    f.flush();
    f.close();
    totalWeatherLogCount++;
  }
}

inline void checkWeatherLogTimer() {
  if (SD.cardType() == CARD_NONE) return;

#if CONFIG_IDF_TARGET_ESP32S3
  // USB 隨身碟活動時暫停寫入 SD 卡，防止 FAT 檔案系統損毀
  if (hasFlag(SysFlag::USB_EXCLUSIVE_LOCK) || (hasFlag(SysFlag::USB_MOUNTED) && (millis() - lastUsbActivity < 5000))) {
    return;
  }
#endif

  if (millis() - lastWeatherLogTime >= (weatherLogIntervalSec * 1000UL)) {
    lastWeatherLogTime = millis();
    appendWeatherRecordCsv();
  }
}

#endif // WEATHER_STATION_H
