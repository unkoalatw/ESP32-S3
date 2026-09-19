#ifndef SDCARD_MANAGER_H
#define SDCARD_MANAGER_H

#include "Config.h"

// ---------------------------------------------------------------------------
// 路徑安全與標準化小幫手 (嚴格防禦路徑穿越 Traversal 與 Null-Byte 注入)
// ---------------------------------------------------------------------------
inline String normalizePath(String path) {
  path.trim();
  if (path.length() == 0) return "/";

  // 清除無效字元與反斜線標準化
  String clean;
  clean.reserve(path.length() + 2);
  for (size_t i = 0; i < path.length(); ++i) {
    char c = path[i];
    if (c == '\0') continue;
    if (c == '\\') c = '/';
    clean += c;
  }

  if (!clean.startsWith("/")) clean = "/" + clean;

  // 消除重複連續斜線 "//"
  while (clean.indexOf("//") >= 0) {
    clean.replace("//", "/");
  }

  // 嚴密防止路徑穿越 (Directory Traversal)
  if (clean == "/.." || clean.startsWith("/../") || clean.endsWith("/..") || clean.indexOf("/../") >= 0 || clean.indexOf("..") >= 0) {
    return "";
  }

  // 移除末尾斜線 (保留根目錄 "/")
  while (clean.length() > 1 && clean.endsWith("/")) {
    clean.remove(clean.length() - 1);
  }

  return clean;
}

inline String leafName(const String &path) {
  String p = path;
  p.replace('\\', '/');
  int idx = p.lastIndexOf('/');
  return (idx >= 0) ? p.substring(idx + 1) : p;
}

inline String parentPath(const String &path) {
  if (path == "/" || path.length() == 0) return "/";
  int idx = path.lastIndexOf('/');
  return (idx <= 0) ? "/" : path.substring(0, idx);
}

inline String joinPath(const String &dir, const String &file) {
  String f = file;
  if (f.startsWith("/")) f.remove(0, 1);
  return (dir == "/" || dir.length() == 0) ? "/" + f : dir + "/" + f;
}

inline bool validName(String name) {
  name.trim();
  if (name.length() == 0 || name == "." || name == ".." || name.indexOf('/') >= 0 || name.indexOf('\\') >= 0) {
    return false;
  }
  for (const char *p = "<>:\"|?*\0"; *p; ++p) {
    if (name.indexOf(*p) >= 0) return false;
  }
  return true;
}

inline String getMIMEType(const String &path) {
  String p = path;
  p.toLowerCase();
  if (p.endsWith(".html") || p.endsWith(".htm")) return "text/html; charset=utf-8";
  if (p.endsWith(".css"))                         return "text/css";
  if (p.endsWith(".js"))                          return "application/javascript";
  if (p.endsWith(".json"))                        return "application/json; charset=utf-8";
  if (p.endsWith(".txt") || p.endsWith(".log") || p.endsWith(".csv") || p.endsWith(".md")) {
    return "text/plain; charset=utf-8";
  }
  if (p.endsWith(".png"))  return "image/png";
  if (p.endsWith(".jpg") || p.endsWith(".jpeg")) return "image/jpeg";
  if (p.endsWith(".gif"))  return "image/gif";
  if (p.endsWith(".webp")) return "image/webp";
  if (p.endsWith(".svg"))  return "image/svg+xml";
  if (p.endsWith(".ico"))  return "image/x-icon";
  if (p.endsWith(".mp4"))  return "video/mp4";
  if (p.endsWith(".webm")) return "video/webm";
  if (p.endsWith(".mov"))  return "video/quicktime";
  if (p.endsWith(".avi"))  return "video/x-msvideo";
  if (p.endsWith(".mkv"))  return "video/x-matroska";
  if (p.endsWith(".mp3"))  return "audio/mpeg";
  if (p.endsWith(".wav"))  return "audio/wav";
  if (p.endsWith(".ogg"))  return "audio/ogg";
  if (p.endsWith(".flac")) return "audio/flac";
  if (p.endsWith(".aac"))  return "audio/aac";
  if (p.endsWith(".pdf"))  return "application/pdf";
  if (p.endsWith(".zip"))  return "application/zip";
  if (p.endsWith(".bin"))  return "application/octet-stream";
  return "application/octet-stream";
}

// ---------------------------------------------------------------------------
// 遞迴檔案與目錄操作 (全面 SpiLock 保護)
// ---------------------------------------------------------------------------
inline bool ensureDirectoryExists(String path) {
  path = normalizePath(path);
  if (path.length() == 0) return false;
  if (path == "/") return true;

  SpiLock lock;
  if (SD.exists(path)) return true;

  int start = 1;
  while (start < (int)path.length()) {
    int idx = path.indexOf('/', start);
    String current = (idx < 0) ? path : path.substring(0, idx);
    if (current.length() > 0 && !SD.exists(current)) {
      if (!SD.mkdir(current)) return false;
    }
    if (idx < 0) break;
    start = idx + 1;
  }
  return true;
}

inline bool removeRecursive(const String &path) {
  auto p = normalizePath(path);
  if (p == "/" || p.length() == 0) return false;

  SpiLock lock;
  if (!SD.exists(p)) return true;

  File node = SD.open(p);
  if (!node) {
    return SD.remove(p) || SD.rmdir(p) || !SD.exists(p);
  }
  if (!node.isDirectory()) {
    node.close();
    return SD.remove(p) || !SD.exists(p);
  }
  node.close();

  // 遍歷目錄項目
  while (true) {
    File dir = SD.open(p);
    if (!dir || !dir.isDirectory()) {
      if (dir) dir.close();
      break;
    }
    File child = dir.openNextFile();
    if (!child) {
      dir.close();
      break;
    }
    String childName = leafName(String(child.name()));
    String childPath = joinPath(p, childName);
    bool isDir = child.isDirectory();
    child.close();
    dir.close();

    if (isDir) {
      removeRecursive(childPath);
    } else {
      SD.remove(childPath);
    }
    yield();
  }

  return SD.rmdir(p) || !SD.exists(p);
}

// ---------------------------------------------------------------------------
// 極速檔案串流 (支援 HTTP 206 Partial Content / Accept-Ranges / 影片進度條拖曳)
// ---------------------------------------------------------------------------
inline void streamFileFast(File &file, const String &contentType) {
  constexpr size_t BUF_SIZE = 8192;
  static uint8_t transferBuffer[BUF_SIZE];

  size_t fileSize = file.size();
  size_t start = 0;
  size_t end = (fileSize > 0) ? (fileSize - 1) : 0;
  int httpCode = 200;

  // 解析 HTTP Range 標頭
  String range = server.hasHeader("Range") ? server.header("Range") : "";
  if (fileSize > 0 && range.startsWith("bytes=")) {
    int dash = range.indexOf('-', 6);
    if (dash > 0) {
      String s = range.substring(6, dash);
      String e = range.substring(dash + 1);
      s.trim();
      e.trim();
      bool valid = false;
      if (s.length() > 0) {
        start = strtoul(s.c_str(), nullptr, 10);
        end = (e.length() > 0) ? strtoul(e.c_str(), nullptr, 10) : (fileSize - 1);
        valid = (start < fileSize);
      } else if (e.length() > 0) {
        size_t n = strtoul(e.c_str(), nullptr, 10);
        if (n > 0) {
          start = (n >= fileSize) ? 0 : (fileSize - n);
          end = fileSize - 1;
          valid = true;
        }
      }
      if (valid) {
        if (end >= fileSize) end = fileSize - 1;
        if (start <= end) {
          httpCode = 206; // Partial Content
          server.sendHeader("Content-Range", "bytes " + String(start) + "-" + String(end) + "/" + String(fileSize));
        } else {
          start = 0;
          end = fileSize - 1;
        }
      }
    }
  }

  size_t totalToSend = (fileSize == 0) ? 0 : (end - start + 1);
  server.sendHeader("Accept-Ranges", "bytes");
  server.setContentLength(totalToSend);
  server.send(httpCode, contentType, "");

  WiFiClient client = server.client();
  if (client && totalToSend > 0) {
    client.setNoDelay(true); // 關閉 Nagle 演算法，減少串流封包抖動
    {
      SpiLock lock;
      if (start > 0) file.seek(start);
    }

    size_t remaining = totalToSend;
    while (remaining > 0 && client.connected()) {
      size_t chunk = (remaining < sizeof(transferBuffer)) ? remaining : sizeof(transferBuffer);
      size_t readCount = 0;
      {
        SpiLock lock;
        readCount = file.read(transferBuffer, chunk);
      }
      if (readCount == 0) break;

      size_t written = client.write(transferBuffer, readCount);
      if (written == 0) break;
      if (written < readCount) {
        SpiLock lock;
        file.seek(file.position() - (readCount - written));
      }
      remaining -= written;
      totalBytesSent += written;
      yield();
    }
  }
}

// ---------------------------------------------------------------------------
// 解壓縮（無壓縮/Store 模式）ZIP 檔案解包引擎
// ---------------------------------------------------------------------------
inline bool unzipFile(const String &zipPath, const String &destDir, int &extracted, int &skipped) {
  SpiLock lock;
  File zip = SD.open(zipPath, FILE_READ);
  if (!zip) return false;

  while (zip.available() >= 30) {
    uint32_t sig = 0;
    zip.read(reinterpret_cast<uint8_t *>(&sig), 4);
    if (sig != 0x04034b50) break;

    zip.seek(zip.position() + 4);
    uint16_t method = 0;
    zip.read(reinterpret_cast<uint8_t *>(&method), 2);
    zip.seek(zip.position() + 8); // Skip Mod Time(2) + Date(2) + CRC(4)

    uint32_t compSize = 0, uncompSize = 0;
    zip.read(reinterpret_cast<uint8_t *>(&compSize), 4);
    zip.read(reinterpret_cast<uint8_t *>(&uncompSize), 4);
    uint16_t nameLen = 0, extraLen = 0;
    zip.read(reinterpret_cast<uint8_t *>(&nameLen), 2);
    zip.read(reinterpret_cast<uint8_t *>(&extraLen), 2);

    char filenameBuf[256];
    uint16_t readLen = (nameLen < sizeof(filenameBuf) - 1) ? nameLen : (sizeof(filenameBuf) - 1);
    zip.read(reinterpret_cast<uint8_t *>(filenameBuf), readLen);
    filenameBuf[readLen] = '\0';
    if (nameLen > readLen) zip.seek(zip.position() + (nameLen - readLen));
    String entryName = String(filenameBuf);

    if (extraLen > 0) zip.seek(zip.position() + extraLen);

    // 擋掉路徑穿越
    if (entryName.indexOf("..") >= 0) {
      zip.seek(zip.position() + compSize);
      skipped++;
      continue;
    }

    String targetPath = joinPath(destDir, entryName);
    if (entryName.endsWith("/")) {
      ensureDirectoryExists(targetPath);
    } else if (method == 0) { // Store 模式 (無壓縮)
      ensureDirectoryExists(parentPath(targetPath));
      File outFile = SD.open(targetPath, FILE_WRITE);
      if (outFile) {
        uint8_t buf[2048];
        uint32_t rem = compSize;
        while (rem > 0 && zip.available()) {
          size_t toRead = (rem > sizeof(buf)) ? sizeof(buf) : rem;
          size_t rd = zip.read(buf, toRead);
          if (rd == 0) break;
          outFile.write(buf, rd);
          rem -= rd;
          yield();
        }
        outFile.flush();
        outFile.close();
        extracted++;
      } else {
        zip.seek(zip.position() + compSize);
        skipped++;
      }
    } else {
      zip.seek(zip.position() + compSize);
      skipped++;
    }
  }
  zip.close();
  return true;
}

// ---------------------------------------------------------------------------
// 健全的多裝置共用 SPI 總線與 MicroSD 卡掛載初始化
// ---------------------------------------------------------------------------
inline bool initSDCard() {
  SpiLock lock;

  // 1. 預先拉高共用 SPI 總線上的所有晶片片選腳位 (De-select All SPI Devices)
  pinMode(LCD_CS, OUTPUT);
  digitalWrite(LCD_CS, HIGH);
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  pinMode(LCD_RST, OUTPUT);
  digitalWrite(LCD_RST, HIGH);

  delay(20);

  // 2. 初始化 SPI 總線 (使用 -1 手動片選，避免硬體佔用 SD_CS)
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, -1);

  // 3. 自動降頻重試機制 (40MHz -> 25MHz -> 20MHz -> 10MHz -> 4MHz)
  const uint32_t freqList[] = { 40000000UL, 25000000UL, 20000000UL, 10000000UL, 4000000UL };
  bool mounted = false;

  for (size_t i = 0; i < sizeof(freqList) / sizeof(freqList[0]); ++i) {
    digitalWrite(SD_CS, HIGH);
    digitalWrite(LCD_CS, HIGH);
    digitalWrite(TOUCH_CS, HIGH);
    delay(10);

    if (SD.begin(SD_CS, SPI, freqList[i])) {
      if (SD.cardType() != CARD_NONE) {
        logf("💾 [SDCard] 記憶卡掛載成功！頻率: %lu MHz, 類型: %s, 容量: %s\n",
             freqList[i] / 1000000UL,
             (SD.cardType() == CARD_SDHC ? "SDHC" : (SD.cardType() == CARD_MMC ? "MMC" : "SD")),
             humanSize((uint64_t)SD.cardSize()).c_str());
        mounted = true;

        // 預先建立全系統核心目錄
        ensureDirectoryExists("/font");
        ensureDirectoryExists("/wiki");
        ensureDirectoryExists("/books");
        ensureDirectoryExists("/photos");
        ensureDirectoryExists("/recordings");
        ensureDirectoryExists("/config");
        ensureDirectoryExists("/weather");
        ensureDirectoryExists("/emergency");
        ensureDirectoryExists("/.trash");
        ensureDirectoryExists("/.versions");
        break;
      }
    }
    SD.end();
    delay(20);
  }

  return mounted;
}

#endif // SDCARD_MANAGER_H
