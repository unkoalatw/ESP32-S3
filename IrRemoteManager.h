#ifndef IR_REMOTE_MANAGER_H
#define IR_REMOTE_MANAGER_H

#include "Config.h"

// ---------------------------------------------------------------------------
// 萬用旗艦級紅外線全波形錄製、硬體 PWM 載波發射與多協定解碼引擎 (D011 TX / D012 RX)
// 支援：NEC 32-bit、Panasonic AC (27-Byte 雙幀)、Sony SIRC (40kHz)、通用 Raw 脈衝
// ---------------------------------------------------------------------------
constexpr size_t MAX_IR_PULSES = 600;
constexpr uint8_t IR_TX_PWM_CHANNEL = 2;
constexpr uint32_t IR_TX_PWM_FREQ    = 38000;
constexpr uint8_t IR_TX_PWM_RES     = 8;

struct IrLearnedSignal {
  uint32_t code = 0;
  String hexStr = "";
  String protocol = "NEC";
  uint16_t bits = 32;
  uint32_t timestamp = 0;
  bool hasData = false;
  uint16_t rawPulses[MAX_IR_PULSES] = {0};
  uint16_t rawLen = 0;
  String analysis = "";
};

struct IrPresetButton {
  char label[32];
  uint32_t code;
  uint32_t code2;
  uint16_t delayMs;
  uint16_t color;
};

extern IrLearnedSignal lastLearnedIr;
extern IrPresetButton g_irPresets[6];
extern volatile bool g_isIrTransmitting;
extern volatile uint32_t g_lastIrTxEndTime;

static uint32_t g_currentIrCarrierFreq = IR_TX_PWM_FREQ;

inline void setIrCarrierFrequency(uint32_t freqHz) {
  if (freqHz < 10000 || freqHz > 100000) freqHz = IR_TX_PWM_FREQ;
  if (freqHz == g_currentIrCarrierFreq) return;
  g_currentIrCarrierFreq = freqHz;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  ledcAttach(IR_TX_PIN, g_currentIrCarrierFreq, IR_TX_PWM_RES);
  ledcWrite(IR_TX_PIN, 0);
#else
  ledcSetup(IR_TX_PWM_CHANNEL, g_currentIrCarrierFreq, IR_TX_PWM_RES);
  ledcWrite(IR_TX_PWM_CHANNEL, 0);
#endif
}

inline void initIrPwm() {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  ledcAttach(IR_TX_PIN, g_currentIrCarrierFreq, IR_TX_PWM_RES);
  ledcWrite(IR_TX_PIN, 0);
#else
  ledcSetup(IR_TX_PWM_CHANNEL, g_currentIrCarrierFreq, IR_TX_PWM_RES);
  ledcAttachPin(IR_TX_PIN, IR_TX_PWM_CHANNEL);
  ledcWrite(IR_TX_PWM_CHANNEL, 0);
#endif
}

inline void sendCarrierPulse(uint32_t microSeconds) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  ledcWrite(IR_TX_PIN, 128); // 50% 佔空比 (8-bit: 128/255)
  delayMicroseconds(microSeconds);
  ledcWrite(IR_TX_PIN, 0);
#else
  ledcWrite(IR_TX_PWM_CHANNEL, 128);
  delayMicroseconds(microSeconds);
  ledcWrite(IR_TX_PWM_CHANNEL, 0);
#endif
}

inline void send38kHzPulse(uint32_t microSeconds) {
  sendCarrierPulse(microSeconds);
}

inline void sendSpace(uint32_t microSeconds) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  ledcWrite(IR_TX_PIN, 0);
#else
  ledcWrite(IR_TX_PWM_CHANNEL, 0);
#endif
  delayMicroseconds(microSeconds);
}

// 發送標準 NEC 32-bit 紅外線編碼 (含 1 次 Repeat Code 強化穿透)
inline void sendNecIrCode(uint32_t code) {
  g_isIrTransmitting = true;

  // 1. 引導碼 (Leader: 9000us Mark + 4500us Space)
  send38kHzPulse(9000);
  sendSpace(4500);

  // 2. 32 位元資料
  for (int i = 0; i < 32; ++i) {
    bool bit = (code >> (31 - i)) & 1;
    send38kHzPulse(560);
    if (bit) {
      sendSpace(1690); // 邏輯 1
    } else {
      sendSpace(560);  // 邏輯 0
    }
  }

  // 3. 結束位元 + 間隔
  send38kHzPulse(560);
  sendSpace(40000);

  // 4. 重發碼 (Repeat Frame: 9ms Mark + 2.25ms Space + 560us Mark)
  send38kHzPulse(9000);
  sendSpace(2250);
  send38kHzPulse(560);
  sendSpace(30000);

  g_lastIrTxEndTime = millis();
  g_isIrTransmitting = false;
  logf("📡 [IR-TX] 成功發射 NEC 32-bit 代碼: 0x%08X\n", (unsigned)code);
}

inline void sendPresetIrCode(int presetIdx) {
  if (presetIdx < 0 || presetIdx >= 6) return;
  const IrPresetButton &btn = g_irPresets[presetIdx];
  if (btn.code != 0) {
    sendNecIrCode(btn.code);
    if (btn.code2 != 0) {
      delay(btn.delayMs > 0 ? btn.delayMs : 100);
      sendNecIrCode(btn.code2);
    }
  }
}

// 發送 Panasonic 國際牌冷氣 27 位元組 (216 位元) 雙幀標準協定
inline void sendPanasonicAcBytes(const uint8_t *bytes, uint8_t len) {
  if (len < 27) return;
  g_isIrTransmitting = true;

  for (int r = 0; r < 2; ++r) {
    // === 第一幀 (Frame 1: 8 Bytes) ===
    send38kHzPulse(3500);
    sendSpace(1750);
    for (int b = 0; b < 8; ++b) {
      uint8_t val = bytes[b];
      for (int bit = 0; bit < 8; ++bit) {
        send38kHzPulse(435);
        if ((val >> bit) & 1) sendSpace(1300);
        else sendSpace(435);
      }
    }
    send38kHzPulse(435);
    sendSpace(10000); // 10ms 幀間靜音

    // === 第二幀 (Frame 2: 19 Bytes) ===
    send38kHzPulse(3500);
    sendSpace(1750);
    for (int b = 8; b < 27; ++b) {
      uint8_t val = bytes[b];
      for (int bit = 0; bit < 8; ++bit) {
        send38kHzPulse(435);
        if ((val >> bit) & 1) sendSpace(1300);
        else sendSpace(435);
      }
    }
    send38kHzPulse(435);
    if (r == 0) sendSpace(40000);
  }

  sendSpace(30000);
  g_lastIrTxEndTime = millis();
  g_isIrTransmitting = false;
  logf("📡 [IR-TX] 成功發射 Panasonic AC 冷氣 27-Byte 標準幀\n");
}

// 發送 Sony SIRC 協定 (12/15/20 位元, 40kHz 載波)
inline void sendSonyIrCode(uint32_t code, uint8_t bits) {
  g_isIrTransmitting = true;
  setIrCarrierFrequency(40000);
  for (int r = 0; r < 3; ++r) {
    sendCarrierPulse(2400);
    sendSpace(600);
    for (int i = 0; i < bits; ++i) {
      bool bit = (code >> i) & 1;
      sendCarrierPulse(bit ? 1200 : 600);
      sendSpace(600);
    }
    if (r < 2) sendSpace(25000);
  }
  sendSpace(30000);
  setIrCarrierFrequency(IR_TX_PWM_FREQ);
  g_lastIrTxEndTime = millis();
  g_isIrTransmitting = false;
  logf("📡 [IR-TX] 成功發射 Sony SIRC 代碼: 0x%X (%d-bit, 40kHz)\n", (unsigned)code, bits);
}

// 發送 Raw 原始微秒脈衝波形
inline void sendRawIrPulses(const uint16_t *pulses, uint16_t count, uint32_t carrierFreq = 38000) {
  if (count < 4) return;
  g_isIrTransmitting = true;
  setIrCarrierFrequency(carrierFreq);

  int repeatCount = (count >= 80) ? 2 : 1;

  for (int r = 0; r < repeatCount; ++r) {
    for (uint16_t i = 0; i < count; ++i) {
      uint32_t dur = pulses[i];
      if ((i % 2) == 0) {
        uint32_t markTime = (dur > 60) ? (dur - 50) : dur;
        sendCarrierPulse(markTime);
      } else {
        uint32_t spaceTime = dur + 50;
        sendSpace(spaceTime);
      }
    }
    if (r + 1 < repeatCount) {
      sendSpace(40000);
    }
  }

  sendSpace(30000);
  setIrCarrierFrequency(IR_TX_PWM_FREQ);
  g_lastIrTxEndTime = millis();
  g_isIrTransmitting = false;
  logf("📡 [IR-TX] 成功發射 Raw 原始波形 (%u 脈衝點, %u Hz)\n", count, (unsigned)carrierFreq);
}

inline void sendRawIrString(const String &rawStr, uint32_t carrierFreq = 38000) {
  if (rawStr.length() < 5) return;
  uint16_t pulses[MAX_IR_PULSES];
  uint16_t count = 0;
  int start = 0;
  while (start < (int)rawStr.length() && count < MAX_IR_PULSES) {
    int comma = rawStr.indexOf(',', start);
    if (comma == -1) comma = rawStr.length();
    String valStr = rawStr.substring(start, comma);
    valStr.trim();
    if (valStr.length() > 0) {
      pulses[count++] = static_cast<uint16_t>(valStr.toInt());
    }
    start = comma + 1;
  }
  if (count > 4) {
    sendRawIrPulses(pulses, count, carrierFreq);
  }
}

inline String analyzeIrCode(uint32_t code) {
  if (code == 0) return "無訊號";

  uint8_t b0 = (code >> 24) & 0xFF;
  uint8_t b1 = (code >> 16) & 0xFF;
  uint8_t b2 = (code >> 8) & 0xFF;
  uint8_t b3 = code & 0xFF;

  bool isStandardNec = ((uint8_t)(b0 ^ b1) == 0xFF) && ((uint8_t)(b2 ^ b3) == 0xFF);
  bool isExtNecCmdValid = ((uint8_t)(b2 ^ b3) == 0xFF);

  String desc = "";
  if (b0 == 0x20 && b1 == 0xDF) {
    desc += "LG / 日韓系影音電視特徵";
  } else if (b0 == 0x00 && b1 == 0xFF) {
    desc += "標準 NEC (投影機 / 風扇 / 燈具)";
  } else if (b0 == 0x88 || b0 == 0x08) {
    desc += "空調冷氣 / 遙控裝置特徵";
  } else if (b0 == 0x02 && b1 == 0xFD) {
    desc += "電視 / 機上盒 (STB) 特徵";
  } else if (isStandardNec) {
    desc += "標準 NEC (Addr: 0x" + String(b0, HEX) + ", Cmd: 0x" + String(b2, HEX) + ")";
  } else if (isExtNecCmdValid) {
    desc += "擴充 NEC (Addr: 0x" + String((b0 << 8) | b1, HEX) + ", Cmd: 0x" + String(b2, HEX) + ")";
  } else {
    desc += "自訂 32-bit NEC 相容格式";
  }

  if (b2 == 0x10 || b2 == 0x12 || b2 == 0x00 || b2 == 0x14) {
    desc += " [電源鍵]";
  } else if (b2 == 0x40 || b2 == 0xC0 || b2 == 0x1A || b2 == 0x1E) {
    desc += " [音量調節]";
  } else if (b2 == 0x00 || b2 == 0x80 || b2 == 0x08 || b2 == 0x18) {
    desc += " [頻道選單]";
  }

  return desc;
}

// 萬用非阻塞紅外線全波形捕獲與多協定解碼器 (D012 接收頭)
inline bool checkIrReceiver() {
  if (g_isIrTransmitting || (millis() - g_lastIrTxEndTime < 180)) {
    return false;
  }

  if (digitalRead(IR_RX_PIN) == LOW) {
    uint16_t pulseBuf[MAX_IR_PULSES];
    uint16_t pulseCount = 0;

    // 1. 全波形連續脈衝採樣
    while (pulseCount < MAX_IR_PULSES) {
      // (a) 量測 LOW 載波脈衝 (Mark)
      uint32_t tStart = micros();
      while (digitalRead(IR_RX_PIN) == LOW) {
        if (micros() - tStart > 30000) break;
      }
      uint32_t markLen = micros() - tStart;
      if (markLen < 70) continue; // 濾除超高頻微小雜訊
      pulseBuf[pulseCount++] = static_cast<uint16_t>(markLen);
      if (pulseCount >= MAX_IR_PULSES || markLen > 30000) break;

      // (b) 量測 HIGH 間隔 (Space)
      tStart = micros();
      while (digitalRead(IR_RX_PIN) == HIGH) {
        if (micros() - tStart > 28000) break;
      }
      uint32_t spaceLen = micros() - tStart;
      if (spaceLen < 70) continue;
      pulseBuf[pulseCount++] = static_cast<uint16_t>(spaceLen);
      if (spaceLen >= 25000) break;
    }

    if (pulseCount >= 8) {
      lastLearnedIr.hasData = true;
      lastLearnedIr.timestamp = millis();
      lastLearnedIr.rawLen = pulseCount;
      memcpy(lastLearnedIr.rawPulses, pulseBuf, pulseCount * sizeof(uint16_t));

      // (A) 標準 NEC 32-bit 解碼
      if (pulseBuf[0] > 4500 && pulseBuf[0] < 12500 && pulseBuf[1] > 2000 && pulseBuf[1] < 6000 && pulseCount >= 64) {
        uint32_t necCode = 0;
        for (int i = 0; i < 32 && (3 + i * 2) < (int)pulseCount; ++i) {
          uint16_t bitSpace = pulseBuf[3 + i * 2];
          necCode <<= 1;
          if (bitSpace > 1000) {
            necCode |= 1;
          }
        }
        lastLearnedIr.code = necCode;
        lastLearnedIr.hexStr = "";
        lastLearnedIr.protocol = "NEC";
        lastLearnedIr.bits = 32;
        lastLearnedIr.analysis = analyzeIrCode(necCode);
        logf("🎯 [IR-Learner] 捕獲 NEC: 0x%08X (%s)\n", (unsigned)necCode, lastLearnedIr.analysis.c_str());
        return true;
      }
      // (B) Panasonic 冷氣雙幀 27-Byte 解碼
      else if (pulseBuf[0] > 2200 && pulseBuf[0] < 5000 && pulseBuf[1] > 1000 && pulseBuf[1] < 5000) {
        uint8_t bytes[32] = {0};
        uint8_t byteIdx = 0;

        int pIdx = 2;
        for (int b = 0; b < 8 && pIdx + 1 < (int)pulseCount; ++b) {
          uint8_t curByte = 0;
          for (int bit = 0; bit < 8 && pIdx + 1 < (int)pulseCount; ++bit) {
            uint16_t s = pulseBuf[pIdx + 1];
            if (s > 800) curByte |= (1 << bit);
            pIdx += 2;
          }
          bytes[byteIdx++] = curByte;
        }

        while (pIdx + 4 < (int)pulseCount && !(pulseBuf[pIdx] > 2200 && pulseBuf[pIdx] < 5000 && pulseBuf[pIdx + 1] > 1000 && pulseBuf[pIdx + 1] < 3000)) {
          pIdx++;
        }
        if (pIdx + 4 < (int)pulseCount && pulseBuf[pIdx] > 2200 && pulseBuf[pIdx] < 5000) {
          pIdx += 2;
          for (int b = 0; b < 19 && pIdx + 1 < (int)pulseCount; ++b) {
            uint8_t curByte = 0;
            for (int bit = 0; bit < 8 && pIdx + 1 < (int)pulseCount; ++bit) {
              uint16_t s = pulseBuf[pIdx + 1];
              if (s > 800) curByte |= (1 << bit);
              pIdx += 2;
            }
            bytes[byteIdx++] = curByte;
          }
        }

        bool isPana = (bytes[0] == 0x02 && bytes[1] == 0x20);
        lastLearnedIr.protocol = isPana ? "Panasonic_AC" : "AirConditioner";
        lastLearnedIr.bits = byteIdx * 8;
        lastLearnedIr.code = ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[13] << 8) | bytes[14];

        String hexDump = "";
        for (int i = 0; i < byteIdx; ++i) {
          if (bytes[i] < 0x10) hexDump += "0";
          hexDump += String(bytes[i], HEX);
        }
        lastLearnedIr.hexStr = hexDump;

        uint8_t calcChecksum = 0;
        if (byteIdx >= 27) {
          for (int i = 8; i < 26; ++i) calcChecksum += bytes[i];
        }
        bool chkOk = (byteIdx >= 27 && calcChecksum == bytes[26]);

        String ana = isPana ? "國際牌冷氣 (Panasonic AC)" : "通用空調冷氣協定";
        if (byteIdx >= 15) {
          bool pwr = (bytes[13] & 0x01) == 0x01;
          uint8_t mode = (bytes[13] >> 4) & 0x07;
          uint8_t temp = (bytes[14] >> 1) & 0x1F;
          if (temp < 16 || temp > 30) temp = 26;

          String mStr = "冷氣";
          if (mode == 0) mStr = "自動";
          else if (mode == 3) mStr = "除濕";
          else if (mode == 4) mStr = "暖氣";
          else if (mode == 6) mStr = "送風";

          ana = String(isPana ? "國際牌冷氣" : "冷氣空調") + " [" + (pwr ? "開機" : "關機") + ", " + String(temp) + "°C, " + mStr + (chkOk ? ", 校驗通過" : "") + "]";
        }

        lastLearnedIr.analysis = ana;
        logf("🎯 [IR-Learner] 捕獲冷氣雙幀 (%u Bytes): %s\n", byteIdx, ana.c_str());
        return true;
      }
      // (C) Sony SIRC 解碼
      else if (pulseBuf[0] > 1800 && pulseBuf[0] < 3000 && pulseBuf[1] > 350 && pulseBuf[1] < 850 && pulseCount >= 24) {
        uint32_t sonyCode = 0;
        uint8_t sBits = 0;
        for (int i = 2; i + 1 < (int)pulseCount && sBits < 20; i += 2) {
          uint16_t m = pulseBuf[i];
          if (m > 900) {
            sonyCode |= (1 << sBits);
          }
          sBits++;
        }
        lastLearnedIr.code = sonyCode;
        lastLearnedIr.hexStr = "";
        lastLearnedIr.protocol = "Sony";
        lastLearnedIr.bits = sBits;
        lastLearnedIr.analysis = "Sony 影音設備 (SIRC " + String(sBits) + "-bit, Cmd: 0x" + String(sonyCode & 0x7F, HEX) + ")";
        logf("🎯 [IR-Learner] 捕獲 Sony 訊號: 0x%X (%d bits)\n", (unsigned)sonyCode, sBits);
        return true;
      }
      // (D) 通用 Raw 脈衝捕獲
      else {
        uint32_t hash = 2166136261UL;
        for (uint16_t i = 0; i < pulseCount; ++i) {
          hash ^= pulseBuf[i];
          hash *= 16777619UL;
        }
        lastLearnedIr.code = hash;
        lastLearnedIr.hexStr = "";
        lastLearnedIr.protocol = "RAW_PULSE";
        lastLearnedIr.bits = pulseCount;

        char anaBuf[64];
        snprintf(anaBuf, sizeof(anaBuf), "通用紅外線訊號 (%u 脈衝, 引導:%uus)", pulseCount, (unsigned)pulseBuf[0]);
        lastLearnedIr.analysis = String(anaBuf);
        logf("🎯 [IR-Learner] 捕獲通用紅外線波形 (%u Pulses, Hash: 0x%08X)\n", pulseCount, (unsigned)hash);
        return true;
      }
    }
  }
  return false;
}

inline void loadIrPresetsFromSD() {
  if (SD.cardType() == CARD_NONE) return;
  SpiLock lock;
  if (!SD.exists("/config/ir_presets.json")) return;
  File f = SD.open("/config/ir_presets.json", FILE_READ);
  if (!f) return;
  String json = f.readString();
  f.close();

  for (int i = 0; i < 6; ++i) {
    String slotKey = "\"slot\":" + String(i);
    int slotPos = json.indexOf(slotKey);
    if (slotPos == -1) slotPos = json.indexOf("\"id\":" + String(i));
    if (slotPos != -1) {
      int labelPos = json.indexOf("\"label\":\"", slotPos);
      if (labelPos != -1 && labelPos < slotPos + 200) {
        int labelStart = labelPos + 9;
        int labelEnd = json.indexOf('"', labelStart);
        if (labelEnd != -1) {
          String lbl = json.substring(labelStart, labelEnd);
          lbl.trim();
          if (lbl.length() > 0) {
            strncpy(g_irPresets[i].label, lbl.c_str(), sizeof(g_irPresets[i].label) - 1);
            g_irPresets[i].label[sizeof(g_irPresets[i].label) - 1] = '\0';
          }
        }
      }
      int codePos = json.indexOf("\"code\":\"", slotPos);
      if (codePos != -1 && codePos < slotPos + 200) {
        int codeStart = codePos + 8;
        int codeEnd = json.indexOf('"', codeStart);
        if (codeEnd != -1) {
          String cStr = json.substring(codeStart, codeEnd);
          cStr.trim();
          if (cStr.length() > 0) {
            g_irPresets[i].code = strtoul(cStr.c_str(), NULL, 16);
          }
        }
      }
      int code2Pos = json.indexOf("\"code2\":\"", slotPos);
      if (code2Pos != -1 && code2Pos < slotPos + 300) {
        int c2Start = code2Pos + 9;
        int c2End = json.indexOf('"', c2Start);
        if (c2End != -1) {
          String c2Str = json.substring(c2Start, c2End);
          c2Str.trim();
          g_irPresets[i].code2 = (c2Str.length() > 0 && c2Str != "0x00000000" && c2Str != "0") ? strtoul(c2Str.c_str(), NULL, 16) : 0;
        }
      }
      int delayPos = json.indexOf("\"delayMs\":", slotPos);
      if (delayPos != -1 && delayPos < slotPos + 350) {
        int dStart = delayPos + 10;
        int dEnd1 = json.indexOf(',', dStart);
        int dEnd2 = json.indexOf('}', dStart);
        int dEnd = (dEnd1 != -1 && (dEnd2 == -1 || dEnd1 < dEnd2)) ? dEnd1 : dEnd2;
        if (dEnd != -1) {
          uint16_t d = json.substring(dStart, dEnd).toInt();
          g_irPresets[i].delayMs = (d > 0) ? d : 700;
        }
      }
    }
  }
}

inline void saveIrPresetsToSD() {
  if (SD.cardType() == CARD_NONE || hasFlag(SysFlag::USB_EXCLUSIVE_LOCK)) return;
  ensureDirectoryExists("/config");
  SpiLock lock;
  File f = SD.open("/config/ir_presets.json", FILE_WRITE);
  if (!f) return;
  String out;
  out.reserve(512);
  out += "[";
  for (int i = 0; i < 6; ++i) {
    char codeHex[16], code2Hex[16];
    snprintf(codeHex, sizeof(codeHex), "0x%08X", (unsigned)g_irPresets[i].code);
    snprintf(code2Hex, sizeof(code2Hex), "0x%08X", (unsigned)g_irPresets[i].code2);
    out += "{\"slot\":" + String(i) + ",\"label\":\"" + jsonEscape(String(g_irPresets[i].label)) + "\",\"code\":\"" + String(codeHex) + "\",\"code2\":\"" + String(code2Hex) + "\",\"delayMs\":" + String(g_irPresets[i].delayMs > 0 ? g_irPresets[i].delayMs : 700) + "}";
    if (i < 5) out += ",";
  }
  out += "]";
  f.print(out);
  f.flush();
  f.close();
}

inline void initIrRemote() {
  initIrPwm();
  pinMode(IR_RX_PIN, INPUT_PULLUP);
  loadIrPresetsFromSD();
  logf("[5/8] 萬用紅外線遙控系統 (TX: GPIO %d, RX: GPIO %d)…… ✅ 啟動完成\n", IR_TX_PIN, IR_RX_PIN);
}

#endif // IR_REMOTE_MANAGER_H
