#ifndef EMERGENCY_MESH_H
#define EMERGENCY_MESH_H

#include "Config.h"
#include "SDCardManager.h"

struct EspNowEmergencyPacket {
  uint32_t msgId;
  char sender[32];
  char location[32];
  char status[16];
  char text[128];
  uint8_t hopCount;
};

extern uint32_t totalEspNowReceived;
extern EspNowEmergencyPacket espNowInbox;

// 建立急難留言板目錄與初始訊息
inline void ensureEmergencyDir() {
  ensureDirectoryExists("/emergency");
  SpiLock lock;
  if (!SD.exists("/emergency/messages.json")) {
    File f = SD.open("/emergency/messages.json", FILE_WRITE);
    if (f) {
      f.println("[\n  {\"id\":1,\"time\":0,\"sender\":\"災難應變指揮中心\",\"status\":\"safe\",\"location\":\"1號廣播庇護站\",\"text\":\"離線急難門戶已啟動！請於留言板回報平安或發布求救與物資需求。\"}\n]");
      f.flush();
      f.close();
    }
  }
}

// ESP-NOW 接收回調 (於 Wi-Fi 高優先任務執行，僅快取封包至信箱，絕不可存取 SD 卡)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
inline void OnEspNowDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len)
#else
inline void OnEspNowDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
#endif
{
  if (len != sizeof(EspNowEmergencyPacket)) return;
  if (hasFlag(SysFlag::ESPNOW_INBOX_FULL)) return; // 前一封尚在主迴圈處理中
  memcpy(&espNowInbox, incomingData, sizeof(espNowInbox));
  setFlag(SysFlag::ESPNOW_INBOX_FULL);
}

inline void initEspNowMesh() {
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(OnEspNowDataRecv);
    logLine("📡 [ESP-NOW Mesh] 離線急難跨跳中繼網絡已啟動！");
  } else {
    logLine("⚠️ [ESP-NOW Mesh] 啟動失敗");
  }
}

// 訊息去重快取，防止廣播風暴
static constexpr size_t SEEN_MSG_CACHE_SIZE = 32;
static uint32_t seenMsgCache[SEEN_MSG_CACHE_SIZE] = {0};
static size_t seenMsgHead = 0;

inline bool isMsgAlreadySeen(uint32_t id) {
  if (id == 0) return false;
  for (size_t i = 0; i < SEEN_MSG_CACHE_SIZE; ++i) {
    if (seenMsgCache[i] == id) return true;
  }
  seenMsgCache[seenMsgHead] = id;
  seenMsgHead = (seenMsgHead + 1) % SEEN_MSG_CACHE_SIZE;
  return false;
}

// 在主迴圈中安全處理急難訊息 (寫入 SD 卡、轉發跨跳)
inline void processEspNowInbox() {
  if (!hasFlag(SysFlag::ESPNOW_INBOX_FULL)) return;

  EspNowEmergencyPacket pkt;
  memcpy(&pkt, &espNowInbox, sizeof(pkt));
  clearFlag(SysFlag::ESPNOW_INBOX_FULL);

  // 防禦無效字串越界
  pkt.sender[sizeof(pkt.sender) - 1]     = '\0';
  pkt.location[sizeof(pkt.location) - 1] = '\0';
  pkt.status[sizeof(pkt.status) - 1]     = '\0';
  pkt.text[sizeof(pkt.text) - 1]         = '\0';

  if (isMsgAlreadySeen(pkt.msgId)) return;

  totalEspNowReceived++;
  logf("📡 [ESP-NOW Mesh] 收到來自「%s」的訊息（Hop:%u）：%s\n", pkt.sender, pkt.hopCount, pkt.text);

  // 寫入 SD 卡急難留言板 (若 USB 隨身碟獨佔寫入中則暫緩寫入，防 FAT 損毀)
  if (SD.cardType() != CARD_NONE && !hasFlag(SysFlag::USB_EXCLUSIVE_LOCK)) {
    ensureEmergencyDir();
    uint32_t nowSec = millis() / 1000;
    String newEntry = "  {\"id\":" + String(pkt.msgId)
                    + ",\"time\":" + String(nowSec)
                    + ",\"sender\":\"" + jsonEscape(pkt.sender) + "\""
                    + ",\"status\":\"" + jsonEscape(pkt.status) + "\""
                    + ",\"location\":\"" + jsonEscape(pkt.location) + "\""
                    + ",\"text\":\"[Mesh-Radio] " + jsonEscape(pkt.text) + "\"}";

    SpiLock lock;
    String existingJson = "";
    if (SD.exists("/emergency/messages.json")) {
      File fRead = SD.open("/emergency/messages.json", FILE_READ);
      if (fRead) {
        existingJson = fRead.readString();
        fRead.close();
      }
    }

    existingJson.trim();
    if (!existingJson.startsWith("[") || !existingJson.endsWith("]")) {
      existingJson = "[\n" + newEntry + "\n]";
    } else {
      int closeIdx = existingJson.lastIndexOf(']');
      if (closeIdx != -1) {
        String prefix = existingJson.substring(0, closeIdx);
        prefix.trim();
        if (prefix == "[") {
          existingJson = "[\n" + newEntry + "\n]";
        } else {
          existingJson = prefix + ",\n" + newEntry + "\n]";
        }
      }
    }

    File fWrite = SD.open("/emergency/messages.json", FILE_WRITE);
    if (fWrite) {
      fWrite.print(existingJson);
      fWrite.flush();
      fWrite.close();
    }
  }

  // 跨跳中繼廣播：若 hopCount < 3，則遞增並轉發
  if (pkt.hopCount < 3) {
    pkt.hopCount++;
    uint8_t broadcastMac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastMac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    if (!esp_now_is_peer_exist(broadcastMac)) {
      esp_now_add_peer(&peerInfo);
    }
    esp_now_send(broadcastMac, (uint8_t *)&pkt, sizeof(pkt));
  }
}

#endif // EMERGENCY_MESH_H
