#ifndef AUDIO_RECORDER_MANAGER_H
#define AUDIO_RECORDER_MANAGER_H

#include "Config.h"
#include <Preferences.h>
#include "driver/i2s.h"

// ---------------------------------------------------------------------------
// 旗艦級 I2S 數位麥克風錄音管理器 (INMP441 / ICS-43434 / MSM261)
// 規格：Master RX、飛利浦標準 I2S、32-bit 對齊、雙聲道時鐘、Core 0 專屬背景任務
// ---------------------------------------------------------------------------
constexpr i2s_port_t I2S_PORT   = I2S_NUM_0;
constexpr uint32_t SAMPLE_RATE  = 16000; // 16kHz 廣播級語音採樣率
constexpr size_t DMA_BUF_COUNT  = 8;
constexpr size_t DMA_BUF_LEN    = 256;

struct AudioRecorderState {
  volatile bool isRecording = false;
  File recordFile;
  String currentFilename = "";
  volatile uint32_t startTime = 0;
  volatile uint32_t totalBytesWritten = 0;
  volatile uint32_t durationSec = 0;
  volatile int16_t lastPeakVolume = 0;
  volatile int32_t lastRawMax = 0;
};

extern AudioRecorderState audioState;

// 寫入標準 44-Byte WAV 檔案標頭 (RIFF Header)
inline void writeWavHeader(File &file, uint32_t totalDataLen) {
  uint32_t totalFileLen = totalDataLen + 36;
  uint16_t numChannels = 1;      // 單聲道 (Mono)
  uint32_t sampleRate = SAMPLE_RATE;
  uint16_t bitsPerSample = 16;   // 16-bit PCM
  uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
  uint16_t blockAlign = numChannels * (bitsPerSample / 8);

  file.seek(0);
  // RIFF Chunk
  file.write((const uint8_t *)"RIFF", 4);
  file.write((const uint8_t *)&totalFileLen, 4);
  file.write((const uint8_t *)"WAVE", 4);

  // fmt Subchunk
  file.write((const uint8_t *)"fmt ", 4);
  uint32_t subchunk1Size = 16;
  file.write((const uint8_t *)&subchunk1Size, 4);
  uint16_t audioFormat = 1; // PCM
  file.write((const uint8_t *)&audioFormat, 2);
  file.write((const uint8_t *)&numChannels, 2);
  file.write((const uint8_t *)&sampleRate, 4);
  file.write((const uint8_t *)&byteRate, 4);
  file.write((const uint8_t *)&blockAlign, 2);
  file.write((const uint8_t *)&bitsPerSample, 2);

  // data Subchunk
  file.write((const uint8_t *)"data", 4);
  file.write((const uint8_t *)&totalDataLen, 4);
}

// ---------------------------------------------------------------------------
// NVS 序號持久化防重疊：生成獨立不碰撞之錄音檔名
// ---------------------------------------------------------------------------
inline uint32_t getNextRecordingIndex() {
  Preferences prefs;
  prefs.begin("audio_rec", false);
  uint32_t index = prefs.getUInt("rec_counter", 0);
  index++;
  if (index == 0) {
    index = 1;
  }
  prefs.putUInt("rec_counter", index);
  prefs.end();
  return index;
}

inline String generateUniqueRecordingPath() {
  char pathBuf[48];
  uint32_t idx = getNextRecordingIndex();
  snprintf(pathBuf, sizeof(pathBuf), "/recordings/REC_%06lu.wav", (unsigned long)idx);

  // 雙重防禦：若 SD 卡上已存在同名檔案 (例如 NVS 被清空)，自動向後遞增直到無衝突
  while (SD.exists(pathBuf)) {
    idx = getNextRecordingIndex();
    snprintf(pathBuf, sizeof(pathBuf), "/recordings/REC_%06lu.wav", (unsigned long)idx);
  }
  return String(pathBuf);
}

constexpr size_t AUDIO_RAM_BUF_SIZE = 2048;
static uint8_t g_audioRamBuf[AUDIO_RAM_BUF_SIZE];
static size_t g_audioRamBufLen = 0;
static TaskHandle_t g_audioTaskHandle = NULL;

// 專屬 FreeRTOS Core 0 音訊背景讀取與 SD 寫入任務
inline void audioRecordingTask(void *pvParameters) {
  int32_t i2s_raw_buffer[128];
  size_t bytes_read = 0;
  int16_t pcm16_buffer[64];

  int32_t dcL = 0;
  int32_t dcR = 0;

  for (;;) {
    esp_err_t result = i2s_read(I2S_PORT, i2s_raw_buffer, sizeof(i2s_raw_buffer), &bytes_read, pdMS_TO_TICKS(100));
    if (result == ESP_OK && bytes_read > 0) {
      size_t total_samples = bytes_read / sizeof(int32_t);
      size_t mono_count = 0;
      int32_t maxAbs = 0;

      for (size_t i = 0; i < total_samples; i += 2) {
        int32_t rawL = i2s_raw_buffer[i];
        int32_t rawR = (i + 1 < total_samples) ? i2s_raw_buffer[i + 1] : 0;

        // 32-bit slot 內 24-bit 對齊轉換，並乘上 8 倍清晰增益
        int32_t sL = (rawL >> 14) * 8;
        int32_t sR = (rawR >> 14) * 8;

        // 即時高通濾波 (DC Offset Filter) 消除直流偏壓
        dcL += (sL - dcL) >> 6;
        dcR += (sR - dcR) >> 6;
        sL -= dcL;
        sR -= dcR;

        // 自動聲道挑選（相容 L/R 接地設為左聲道，或接 3.3V 設為右聲道）
        int32_t s = (abs(sL) >= abs(sR)) ? sL : sR;

        // 軟削峰保真防破音保護
        if (s > 32767) s = 32767;
        else if (s < -32768) s = -32768;

        pcm16_buffer[mono_count++] = static_cast<int16_t>(s);

        int32_t absS = abs(s);
        if (absS > maxAbs) maxAbs = absS;
      }

      // 計算 0~100% 動態 VU 表跳動響應
      audioState.lastRawMax = maxAbs;
      int normPeak = (maxAbs * 100) / 2500;
      if (normPeak > 100) normPeak = 100;
      audioState.lastPeakVolume = static_cast<int16_t>(normPeak);

      // 若處於錄音狀態且未處於 USB MSC 獨佔模式，累計寫入緩衝區
      if (audioState.isRecording && audioState.recordFile && !hasFlag(SysFlag::USB_EXCLUSIVE_LOCK)) {
        size_t incomingBytes = mono_count * sizeof(int16_t);
        if (g_audioRamBufLen + incomingBytes <= AUDIO_RAM_BUF_SIZE) {
          memcpy(g_audioRamBuf + g_audioRamBufLen, pcm16_buffer, incomingBytes);
          g_audioRamBufLen += incomingBytes;
        }

        if (g_audioRamBufLen >= 1024) {
          SpiLock lock(pdMS_TO_TICKS(100));
          if (lock.isLocked() && audioState.isRecording && audioState.recordFile) {
            digitalWrite(LCD_CS, HIGH);
            digitalWrite(TOUCH_CS, HIGH);
            size_t written = audioState.recordFile.write(g_audioRamBuf, g_audioRamBufLen);
            audioState.totalBytesWritten += written;
            g_audioRamBufLen = 0;
            digitalWrite(SD_CS, HIGH);
          }
        }

        uint32_t nowMs = millis();
        audioState.durationSec = (nowMs - audioState.startTime) / 1000;

        // 1. 掉電防護：每 5 秒同步更新 WAV 標頭並 flush
        static uint32_t s_lastHeaderSyncMs = 0;
        if (nowMs - s_lastHeaderSyncMs >= 5000) {
          s_lastHeaderSyncMs = nowMs;
          SpiLock lock(pdMS_TO_TICKS(100));
          if (lock.isLocked() && audioState.isRecording && audioState.recordFile) {
            digitalWrite(LCD_CS, HIGH);
            digitalWrite(TOUCH_CS, HIGH);
            uint32_t curPos = audioState.recordFile.position();
            writeWavHeader(audioState.recordFile, audioState.totalBytesWritten);
            audioState.recordFile.seek(curPos);
            audioState.recordFile.flush();
            digitalWrite(SD_CS, HIGH);
          }
        }

        // 2. 自動滾動分段：每 15 分鐘 (900秒) 先 flush 殘留 RAM 緩衝區再切換下一檔
        if (audioState.durationSec >= 900) {
          SpiLock lock(pdMS_TO_TICKS(200));
          if (lock.isLocked() && audioState.isRecording && audioState.recordFile) {
            digitalWrite(LCD_CS, HIGH);
            digitalWrite(TOUCH_CS, HIGH);

            // 切檔前徹底將 RAM 緩衝區殘留音訊寫入舊檔案，消除檔案交界資料錯位
            if (g_audioRamBufLen > 0) {
              size_t flushed = audioState.recordFile.write(g_audioRamBuf, g_audioRamBufLen);
              audioState.totalBytesWritten += flushed;
              g_audioRamBufLen = 0;
            }

            writeWavHeader(audioState.recordFile, audioState.totalBytesWritten);
            audioState.recordFile.flush();
            audioState.recordFile.close();

            audioState.currentFilename = generateUniqueRecordingPath();
            audioState.recordFile = SD.open(audioState.currentFilename.c_str(), FILE_WRITE);
            if (audioState.recordFile) {
              uint8_t dummyHeader[44] = {0};
              audioState.recordFile.write(dummyHeader, 44);
              audioState.totalBytesWritten = 0;
              audioState.startTime = millis();
              audioState.durationSec = 0;
              logf("🎙️ [AUDIO] 自動分段滾動錄音 (無縫切檔): %s\n", audioState.currentFilename.c_str());
            }
            digitalWrite(SD_CS, HIGH);
          }
        }
      }
    }
  }
}

inline void initI2SMicrophone() {
  i2s_config_t i2s_config = {
    .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = static_cast<i2s_comm_format_t>(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = DMA_BUF_COUNT,
    .dma_buf_len = DMA_BUF_LEN,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = MIC_I2S_SCK,
    .ws_io_num = MIC_I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = MIC_I2S_SD
  };

  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err == ESP_OK) {
    i2s_set_pin(I2S_PORT, &pin_config);
    i2s_zero_dma_buffer(I2S_PORT);

    if (g_audioTaskHandle == NULL) {
      xTaskCreatePinnedToCore(
        audioRecordingTask,
        "i2s_mic_task",
        4096,
        NULL,
        2,
        &g_audioTaskHandle,
        0 // 綁定至 Core 0，讓 Core 1 專注處理 WebServer 與螢幕顯示
      );
    }
    logf("[6/8] I2S 數位高解析麥克風系統 (SD: GPIO %d, SCK: GPIO %d, WS: GPIO %d)…… ✅ 啟動完成\n", MIC_I2S_SD, MIC_I2S_SCK, MIC_I2S_WS);
  } else {
    logf("❌ [AUDIO] I2S 驅動初始化失敗: %d\n", err);
  }
}

inline bool startAudioRecording() {
  if (audioState.isRecording) return true;
  if (hasFlag(SysFlag::USB_EXCLUSIVE_LOCK)) {
    logLine("❌ [AUDIO] USB 隨身碟模式獨佔中，無法開啟錄音！");
    return false;
  }
  if (SD.cardType() == CARD_NONE) {
    logLine("❌ [AUDIO] 未偵測到 SD 卡，無法開始錄音！");
    return false;
  }

  ensureDirectoryExists("/recordings");

  SpiLock lock;
  digitalWrite(LCD_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);

  audioState.currentFilename = generateUniqueRecordingPath();
  audioState.recordFile = SD.open(audioState.currentFilename.c_str(), FILE_WRITE);

  if (!audioState.recordFile) {
    logLine("❌ [AUDIO] 無法建立 SD 卡錄音檔！");
    return false;
  }

  uint8_t dummyHeader[44] = {0};
  audioState.recordFile.write(dummyHeader, 44);
  audioState.totalBytesWritten = 0;
  audioState.startTime = millis();
  audioState.durationSec = 0;
  g_audioRamBufLen = 0;
  audioState.isRecording = true;
  logf("🎙️ [AUDIO] 開始會議/課堂錄音: %s\n", audioState.currentFilename.c_str());
  return true;
}

inline void stopAudioRecording() {
  if (!audioState.isRecording) return;
  audioState.isRecording = false;

  SpiLock lock;
  digitalWrite(LCD_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);

  if (audioState.recordFile) {
    if (g_audioRamBufLen > 0) {
      audioState.recordFile.write(g_audioRamBuf, g_audioRamBufLen);
      audioState.totalBytesWritten += g_audioRamBufLen;
      g_audioRamBufLen = 0;
    }
    writeWavHeader(audioState.recordFile, audioState.totalBytesWritten);
    audioState.recordFile.flush();
    audioState.recordFile.close();
    logf("🎙️ [AUDIO] 錄音完成！檔案大小: %u bytes (路徑: %s)\n", (unsigned)audioState.totalBytesWritten, audioState.currentFilename.c_str());
  }
}

inline void handleAudioRecorderLoop() {
  // FreeRTOS background task on Core 0 processes audio continuously
}

#endif // AUDIO_RECORDER_MANAGER_H
