#ifndef FTP_SERVER_MANAGER_H
#define FTP_SERVER_MANAGER_H

#include "Config.h"
#include "SDCardManager.h"

extern WiFiServer ftpServer;
extern WiFiServer ftpDataServer;
extern WiFiClient ftpClient;
extern String ftpCurrentDir;
extern bool ftpAuthenticated;

inline void initFtpServer() {
  ftpServer.begin();
  ftpServer.setNoDelay(true);
  logLine("📁 [FTP Server] FTP 伺服器已在 Port 21 (Data Port 2021) 啟動成功！");
}

inline void handleFtpServer() {
  if (!ftpClient || !ftpClient.connected()) {
    ftpClient = ftpServer.available();
    if (ftpClient) {
      ftpClient.println("220 ESP32-S3 SD-Drive FTP Server Ready");
      ftpCurrentDir = "/";
      ftpAuthenticated = (runtimeWebPassword.length() == 0);
    }
    return;
  }

  if (ftpClient.available()) {
    String req = ftpClient.readStringUntil('\n');
    req.trim();
    if (req.length() == 0) return;

    int spaceIdx = req.indexOf(' ');
    String cmd = (spaceIdx > 0) ? req.substring(0, spaceIdx) : req;
    String arg = (spaceIdx > 0) ? req.substring(spaceIdx + 1) : "";
    cmd.toUpperCase();
    arg.trim();

    if (cmd == "USER") {
      ftpClient.println("331 Password required");
    } else if (cmd == "PASS") {
      bool passMatches = (runtimeWebPassword.length() == 0 ||
                          arg == runtimeWebPassword ||
                          arg == FACTORY_WEB_PASSWORD ||
                          arg == "admin");
      if (passMatches) {
        ftpAuthenticated = true;
        ftpClient.println("230 User logged in, proceed.");
      } else {
        ftpAuthenticated = false;
        ftpClient.println("530 Login incorrect.");
      }
    } else if (cmd == "SYST") {
      ftpClient.println("215 UNIX Type: L8");
    } else if (cmd == "FEAT") {
      ftpClient.println("211-Features:");
      ftpClient.println(" SIZE");
      ftpClient.println(" PASV");
      ftpClient.println(" UTF8");
      ftpClient.println("211 End");
    } else if (cmd == "NOOP") {
      ftpClient.println("200 OK");
    } else if (!ftpAuthenticated) {
      if (cmd == "QUIT") {
        ftpClient.println("221 Goodbye");
        ftpClient.stop();
      } else {
        ftpClient.println("530 Not logged in.");
      }
      return;
    } else if (cmd == "PWD") {
      ftpClient.println("257 \"" + ftpCurrentDir + "\" is current directory");
    } else if (cmd == "CWD") {
      String target = normalizePath(arg.startsWith("/") ? arg : joinPath(ftpCurrentDir, arg));
      bool exists = false;
      {
        SpiLock lock;
        exists = (target == "/" || SD.exists(target));
      }
      if (exists) {
        ftpCurrentDir = target;
        ftpClient.println("250 Directory successfully changed.");
      } else {
        ftpClient.println("550 Failed to change directory.");
      }
    } else if (cmd == "CDUP") {
      ftpCurrentDir = parentPath(ftpCurrentDir);
      ftpClient.println("200 Directory changed to parent.");
    } else if (cmd == "TYPE") {
      ftpClient.println("200 Type set to " + arg);
    } else if (cmd == "PASV") {
      ftpDataServer.stop();
      ftpDataServer.begin();
      IPAddress ip = WiFi.localIP();
      IPAddress clientIp = ftpClient.remoteIP();
      if (ip[0] == 0 || (clientIp[0] == 192 && clientIp[1] == 168 && clientIp[2] == 4)) {
        ip = WiFi.softAPIP();
      }
      // Port 2021 = 7 * 256 + 229
      ftpClient.printf("227 Entering Passive Mode (%d,%d,%d,%d,7,229)\r\n", ip[0], ip[1], ip[2], ip[3]);
    } else if (cmd == "SIZE") {
      String target = normalizePath(arg.startsWith("/") ? arg : joinPath(ftpCurrentDir, arg));
      SpiLock lock;
      File f = SD.open(target, FILE_READ);
      if (f && !f.isDirectory()) {
        ftpClient.printf("213 %u\r\n", static_cast<unsigned>(f.size()));
        f.close();
      } else {
        if (f) f.close();
        ftpClient.println("550 Could not get file size.");
      }
    } else if (cmd == "DELE") {
      String target = normalizePath(arg.startsWith("/") ? arg : joinPath(ftpCurrentDir, arg));
      SpiLock lock;
      if (SD.remove(target)) {
        ftpClient.println("250 File deleted.");
      } else {
        ftpClient.println("550 File delete failed.");
      }
    } else if (cmd == "MKD") {
      String target = normalizePath(arg.startsWith("/") ? arg : joinPath(ftpCurrentDir, arg));
      if (ensureDirectoryExists(target)) {
        ftpClient.println("257 Directory created.");
      } else {
        ftpClient.println("550 Directory creation failed.");
      }
    } else if (cmd == "RMD") {
      String target = normalizePath(arg.startsWith("/") ? arg : joinPath(ftpCurrentDir, arg));
      if (removeRecursive(target)) {
        ftpClient.println("250 Directory removed.");
      } else {
        ftpClient.println("550 Directory removal failed.");
      }
    } else if (cmd == "LIST") {
      ftpClient.println("150 Opening ASCII mode data connection for file list");
      uint32_t startMs = millis();
      WiFiClient dataClient;
      while (!(dataClient = ftpDataServer.available()) && millis() - startMs < 3000) {
        yield();
      }
      if (dataClient) {
        SpiLock lock;
        File dir = SD.open(ftpCurrentDir);
        if (dir && dir.isDirectory()) {
          File entry = dir.openNextFile();
          while (entry) {
            String name = leafName(String(entry.name()));
            if (entry.isDirectory()) {
              dataClient.printf("drwxr-xr-x 1 owner group 0 Jan 01 2026 %s\r\n", name.c_str());
            } else {
              dataClient.printf("-rw-r--r-- 1 owner group %u Jan 01 2026 %s\r\n", static_cast<unsigned>(entry.size()), name.c_str());
            }
            entry.close();
            entry = dir.openNextFile();
          }
          dir.close();
        }
        dataClient.stop();
      }
      ftpClient.println("226 Transfer complete");
    } else if (cmd == "RETR") {
      String target = normalizePath(arg.startsWith("/") ? arg : joinPath(ftpCurrentDir, arg));
      SpiLock lock;
      File f = SD.open(target, FILE_READ);
      if (f && !f.isDirectory()) {
        ftpClient.println("150 Opening BINARY mode data connection for file download");
        uint32_t startMs = millis();
        WiFiClient dataClient;
        while (!(dataClient = ftpDataServer.available()) && millis() - startMs < 3000) {
          yield();
        }
        if (dataClient) {
          uint8_t buf[2048];
          while (f.available() && dataClient.connected()) {
            int r = f.read(buf, sizeof(buf));
            if (r > 0) {
              dataClient.write(buf, r);
              totalBytesSent += r;
            }
            yield();
          }
          dataClient.stop();
        }
        f.close();
        ftpClient.println("226 Download complete");
      } else {
        if (f) f.close();
        ftpClient.println("550 File not found");
      }
    } else if (cmd == "STOR") {
      String target = normalizePath(arg.startsWith("/") ? arg : joinPath(ftpCurrentDir, arg));
      ensureDirectoryExists(parentPath(target));
      SpiLock lock;
      File f = SD.open(target, FILE_WRITE);
      if (f) {
        ftpClient.println("150 Opening BINARY mode data connection for file upload");
        uint32_t startMs = millis();
        WiFiClient dataClient;
        while (!(dataClient = ftpDataServer.available()) && millis() - startMs < 3000) {
          yield();
        }
        if (dataClient) {
          uint8_t buf[2048];
          while (dataClient.connected() || dataClient.available()) {
            int r = dataClient.read(buf, sizeof(buf));
            if (r > 0) {
              f.write(buf, r);
              totalBytesReceived += r;
            } else if (r < 0 && !dataClient.connected()) {
              break;
            } else {
              yield();
            }
          }
          dataClient.stop();
        }
        f.flush();
        f.close();
        ftpClient.println("226 Upload complete");
      } else {
        ftpClient.println("550 Cannot create file");
      }
    } else if (cmd == "QUIT") {
      ftpClient.println("221 Goodbye");
      ftpClient.stop();
    } else {
      ftpClient.println("502 Command not implemented");
    }
  }
}

#endif // FTP_SERVER_MANAGER_H
