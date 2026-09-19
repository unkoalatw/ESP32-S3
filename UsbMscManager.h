#ifndef USB_MSC_MANAGER_H
#define USB_MSC_MANAGER_H

#include "Config.h"

#if CONFIG_IDF_TARGET_ESP32S3
#include "USB.h"
#include "USBMSC.h"

extern USBMSC mscDrive;

inline bool onStartStopMSC(uint8_t power_condition, bool start, bool load_eject) {
  (void)power_condition;
  if (load_eject && !start) {
    clearFlag(SysFlag::USB_MOUNTED);
    clearFlag(SysFlag::USB_EXCLUSIVE_LOCK);
  } else if (start) {
    setFlag(SysFlag::USB_MOUNTED);
  }
  return true;
}

inline int32_t onReadMSC(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  (void)offset;
  setFlag(SysFlag::USB_MOUNTED);
  lastUsbActivity = millis();
  uint8_t *buf = (uint8_t *)buffer;
  uint32_t sectorCount = bufsize / 512;

  // 使用有界限超時 (100ms) 取得 SPI 總線鎖，防止 USB Task 死鎖造成看門狗重啟
  SpiLock lock(pdMS_TO_TICKS(100));
  if (!lock.isLocked()) {
    return -1;
  }

  digitalWrite(LCD_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);

  int32_t bytesRead = 0;
  for (uint32_t i = 0; i < sectorCount; i++) {
    if (!SD.readRAW(buf + (i * 512), lba + i)) {
      digitalWrite(SD_CS, HIGH);
      return -1;
    }
    bytesRead += 512;
  }

  digitalWrite(SD_CS, HIGH);
  return bytesRead;
}

inline int32_t onWriteMSC(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
  (void)offset;
  setFlag(SysFlag::USB_MOUNTED);
  setFlag(SysFlag::USB_EXCLUSIVE_LOCK); // 標記 USB 正在寫入 FAT 磁區，指示 ESP32 內部暫停寫入
  lastUsbActivity = millis();
  uint8_t *buf = (uint8_t *)buffer;
  uint32_t sectorCount = bufsize / 512;

  SpiLock lock(pdMS_TO_TICKS(100));
  if (!lock.isLocked()) {
    return -1;
  }

  digitalWrite(LCD_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);

  int32_t bytesWritten = 0;
  for (uint32_t i = 0; i < sectorCount; i++) {
    if (!SD.writeRAW(buf + (i * 512), lba + i)) {
      digitalWrite(SD_CS, HIGH);
      return -1;
    }
    bytesWritten += 512;
  }

  digitalWrite(SD_CS, HIGH);
  return bytesWritten;
}

inline void initUSBMSCDrive() {
  uint64_t cardSizeBytes = SD.cardSize();
  uint32_t secCount = 0;
  if (cardSizeBytes > 0) {
    secCount = static_cast<uint32_t>(cardSizeBytes / 512ULL);
  } else {
    uint64_t totalFsBytes = SD.totalBytes();
    if (totalFsBytes > 0) {
      secCount = static_cast<uint32_t>(totalFsBytes / 512ULL);
    }
  }

  logf("💾 [USB MSC] 記憶卡容量: %llu Bytes -> 總磁區數: %lu\n", cardSizeBytes, (unsigned long)secCount);

  if (secCount == 0) {
    logLine("⚠️ [USB MSC] 無法取得有效 SD 磁區數，暫緩啟動隨身碟模式");
    return;
  }

  mscDrive.vendorID("ESP32-S3");
  mscDrive.productID("SD-Drive");
  mscDrive.productRevision("1.0");
  mscDrive.onStartStop(onStartStopMSC);
  mscDrive.onRead(onReadMSC);
  mscDrive.onWrite(onWriteMSC);
  mscDrive.mediaPresent(true);
  mscDrive.begin(secCount, 512);
  USB.begin();
  logLine("[3/8] USB 隨身碟模式…… ✅ 已啟動（電腦插上 USB 線即可讀取）");
}
#endif

#endif // USB_MSC_MANAGER_H
