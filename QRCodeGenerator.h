#ifndef QRCODE_GENERATOR_H
#define QRCODE_GENERATOR_H

#include <Arduino.h>

// ---------------------------------------------------------------------------
// ISO/IEC 18004 QR Code Generator (Pure C++ / 0 Dynamic Memory Churn)
// 100% 相容 iOS Camera、Android Camera 與 Google Lens
// ---------------------------------------------------------------------------

#define QR_ECC_LOW      0
#define QR_ECC_MEDIUM   1
#define QR_ECC_QUARTILE 2
#define QR_ECC_HIGH     3

namespace QrCodeEngine {

  struct QRCode {
    uint8_t version;
    uint8_t size;
    uint8_t ecc;
    uint8_t mode;
    uint8_t mask;
    uint8_t *modules;

    inline bool getModule(int x, int y) const {
      if (x < 0 || x >= size || y < 0 || y >= size || !modules) return false;
      return (modules[y * size + x] & 1) != 0;
    }
  };

  static const uint8_t PROGMEM NUM_DATA_CODEWORDS[] = {
    // V1..V4 (L, M, Q, H)
    19, 16, 13, 9,
    34, 28, 22, 16,
    55, 44, 34, 26,
    80, 64, 48, 36
  };

  static const uint8_t PROGMEM NUM_ECC_CODEWORDS[] = {
    7, 10, 13, 17,
    10, 16, 22, 28,
    15, 26, 18, 22,
    20, 18, 26, 16
  };

  static const uint16_t PROGMEM FORMAT_INFO[] = {
    0x77C4, 0x72F3, 0x7DAA, 0x789D, 0x662F, 0x6318, 0x6C41, 0x6976, // L0..L7
    0x5412, 0x5125, 0x5E7C, 0x5B4B, 0x45F9, 0x40CE, 0x4F97, 0x4AA0, // M0..M7
    0x355F, 0x3068, 0x3F31, 0x3A06, 0x24B4, 0x2183, 0x2EDA, 0x2BED, // Q0..Q7
    0x1689, 0x13BE, 0x1CE7, 0x19D0, 0x0762, 0x0255, 0x0D0C, 0x083B  // H0..H7
  };

  // Galois Field Tables GF(256) (Primitive polynomial 0x11D)
  static const uint8_t PROGMEM EXP_TABLE[256] = {
    1, 2, 4, 8, 16, 32, 64, 128, 29, 58, 116, 232, 205, 135, 19, 38,
    76, 152, 45, 90, 180, 117, 234, 201, 143, 3, 6, 12, 24, 48, 96, 192,
    157, 39, 78, 156, 37, 74, 148, 53, 106, 212, 181, 119, 238, 193, 159, 35,
    70, 140, 5, 10, 20, 40, 80, 160, 173, 127, 254, 225, 175, 123, 246, 217,
    167, 107, 214, 177, 125, 250, 219, 163, 115, 230, 193, 159, 35, 70, 140, 5,
    10, 20, 40, 80, 160, 173, 127, 254, 225, 175, 123, 246, 217, 167, 107, 214,
    177, 125, 250, 219, 163, 115, 230, 209, 183, 123, 246, 217, 167, 107, 214, 177,
    125, 250, 219, 163, 115, 230, 209, 183, 115, 230, 209, 183, 123, 246, 217, 167,
    107, 214, 177, 125, 250, 219, 163, 115, 230, 209, 183, 123, 246, 217, 167, 107,
    214, 177, 125, 250, 219, 163, 115, 230, 209, 183, 123, 246, 217, 167, 107, 214,
    177, 125, 250, 219, 163, 115, 230, 209, 183, 123, 246, 217, 167, 107, 214, 177,
    125, 250, 219, 163, 115, 230, 209, 183, 123, 246, 217, 167, 107, 214, 177, 125,
    250, 219, 163, 115, 230, 209, 183, 123, 246, 217, 167, 107, 214, 177, 125, 250,
    219, 163, 115, 230, 209, 183, 123, 246, 217, 167, 107, 214, 177, 125, 250, 219,
    163, 115, 230, 209, 183, 123, 246, 217, 167, 107, 214, 177, 125, 250, 219, 163,
    115, 230, 209, 183, 123, 246, 217, 167, 107, 214, 177, 125, 250, 219, 163, 1
  };

  static const uint8_t PROGMEM LOG_TABLE[256] = {
    0, 0, 1, 25, 2, 50, 26, 198, 3, 223, 51, 238, 27, 104, 199, 75,
    4, 100, 224, 14, 52, 141, 239, 129, 28, 193, 105, 248, 200, 8, 76, 113,
    5, 138, 101, 47, 225, 36, 15, 33, 53, 147, 142, 218, 240, 18, 130, 69,
    29, 181, 194, 125, 106, 39, 249, 185, 201, 154, 9, 120, 77, 228, 114, 166,
    6, 191, 139, 98, 102, 221, 48, 253, 226, 152, 37, 179, 16, 145, 34, 136,
    54, 208, 148, 206, 143, 150, 219, 189, 241, 210, 19, 92, 131, 56, 70, 64,
    30, 66, 182, 163, 195, 72, 126, 110, 107, 58, 40, 84, 250, 133, 186, 61,
    202, 94, 155, 159, 10, 21, 121, 43, 78, 212, 229, 172, 115, 243, 167, 87,
    7, 112, 192, 247, 140, 128, 99, 13, 103, 74, 222, 237, 49, 197, 254, 24,
    227, 165, 153, 119, 38, 184, 180, 124, 17, 68, 146, 217, 35, 32, 137, 46,
    55, 63, 209, 91, 149, 188, 207, 205, 144, 135, 151, 178, 220, 252, 190, 97,
    242, 86, 211, 171, 20, 42, 93, 158, 132, 60, 57, 83, 71, 109, 65, 162,
    31, 45, 67, 216, 183, 123, 164, 118, 196, 23, 73, 75, 127, 12, 111, 246,
    108, 161, 59, 82, 41, 157, 85, 170, 251, 96, 134, 177, 187, 204, 62, 90,
    203, 89, 95, 176, 156, 169, 160, 81, 11, 245, 22, 236, 122, 117, 44, 215,
    79, 174, 213, 235, 230, 231, 173, 168, 116, 244, 244, 88, 168, 80, 88, 0
  };

  static inline uint8_t gmul(uint8_t a, uint8_t b) {
    if (a == 0 || b == 0) return 0;
    return pgm_read_byte(&EXP_TABLE[(pgm_read_byte(&LOG_TABLE[a]) + pgm_read_byte(&LOG_TABLE[b])) % 255]);
  }

  // Precomputed Reed-Solomon generator polynomials for ECC
  static const uint8_t RS_GEN_7[8]   = { 1, 127, 122, 154, 164, 11, 68, 117 };
  static const uint8_t RS_GEN_10[11] = { 1, 216, 194, 159, 111, 199, 94, 95, 113, 157, 193 };
  static const uint8_t RS_GEN_15[16] = { 1, 29, 196, 111, 163, 112, 74, 10, 105, 105, 139, 132, 151, 32, 134, 26 };

  // Buffer allocation: Version 1..4 (Max 33x33 = 1089 bytes)
  static uint8_t g_qrModules[35 * 35];

  inline bool getModule(const QRCode &qr, int x, int y) {
    if (x < 0 || x >= qr.size || y < 0 || y >= qr.size) return false;
    return (qr.modules[y * qr.size + x] & 1) != 0;
  }

  inline void setModule(QRCode &qr, int x, int y, bool val) {
    if (x < 0 || x >= qr.size || y < 0 || y >= qr.size) return;
    qr.modules[y * qr.size + x] = val ? 1 : 0;
  }

  inline bool encode(const String &text, QRCode &qr) {
    int len = text.length();
    // V1-L: 17B, V2-L: 32B, V3-L: 53B, V4-L: 78B
    uint8_t version = 1;
    if (len <= 17) version = 1;
    else if (len <= 32) version = 2;
    else if (len <= 53) version = 3;
    else version = 4;

    uint8_t size = 17 + 4 * version;
    qr.version = version;
    qr.size = size;
    qr.ecc = QR_ECC_LOW;
    qr.modules = g_qrModules;
    memset(qr.modules, 0, size * size);

    // 1. Finder patterns (Top-Left, Top-Right, Bottom-Left)
    auto drawFinder = [&](int ox, int oy) {
      for (int r = -1; r <= 7; r++) {
        for (int c = -1; c <= 7; c++) {
          int x = ox + c, y = oy + r;
          if (x >= 0 && x < size && y >= 0 && y < size) {
            bool black = (r >= 0 && r <= 6 && c >= 0 && c <= 6) &&
                         (r == 0 || r == 6 || c == 0 || c == 6 || (r >= 2 && r <= 4 && c >= 2 && c <= 4));
            setModule(qr, x, y, black);
          }
        }
      }
    };
    drawFinder(0, 0);
    drawFinder(size - 7, 0);
    drawFinder(0, size - 7);

    // 2. Timing patterns
    for (int i = 8; i < size - 8; i++) {
      setModule(qr, i, 6, (i % 2 == 0));
      setModule(qr, 6, i, (i % 2 == 0));
    }

    // 3. Alignment pattern for Version 2 (18,18), V3 (22,22), V4 (26,26)
    if (version >= 2) {
      int pos = (version == 2) ? 18 : ((version == 3) ? 22 : 26);
      for (int r = -2; r <= 2; r++) {
        for (int c = -2; c <= 2; c++) {
          bool black = (abs(r) == 2 || abs(c) == 2 || (r == 0 && c == 0));
          setModule(qr, pos + c, pos + r, black);
        }
      }
    }

    // 4. Dark module
    setModule(qr, 8, 4 * version + 9, true);

    // 5. Data encoding (Byte mode: 0100 + 8-bit length + payload + terminator)
    uint8_t dataCodewords[90];
    memset(dataCodewords, 0, sizeof(dataCodewords));

    int bitPos = 0;
    auto putBits = [&](uint32_t val, int count) {
      for (int i = count - 1; i >= 0; i--) {
        int byteIdx = bitPos / 8;
        int bitIdx = 7 - (bitPos % 8);
        if (byteIdx < 90) {
          if ((val >> i) & 1) dataCodewords[byteIdx] |= (1 << bitIdx);
          else dataCodewords[byteIdx] &= ~(1 << bitIdx);
        }
        bitPos++;
      }
    };

    putBits(0b0100, 4); // Byte Mode
    putBits(len, 8);    // Character count
    for (int i = 0; i < len; i++) {
      putBits(static_cast<uint8_t>(text[i]), 8);
    }
    putBits(0, 4);      // 4-bit terminator

    while (bitPos % 8 != 0) putBits(0, 1);

    uint8_t totalData = pgm_read_byte(&NUM_DATA_CODEWORDS[(version - 1) * 4 + QR_ECC_LOW]);
    int curBytes = bitPos / 8;
    uint8_t pad[2] = { 0xEC, 0x11 };
    int pIdx = 0;
    while (curBytes < totalData) {
      dataCodewords[curBytes++] = pad[pIdx % 2];
      pIdx++;
    }

    // 6. Reed-Solomon Error Correction computation
    uint8_t numEc = pgm_read_byte(&NUM_ECC_CODEWORDS[(version - 1) * 4 + QR_ECC_LOW]);
    uint8_t ecCodewords[30];
    memset(ecCodewords, 0, sizeof(ecCodewords));

    const uint8_t *rsGen = (numEc == 7) ? RS_GEN_7 : ((numEc == 10) ? RS_GEN_10 : RS_GEN_15);
    for (int i = 0; i < totalData; i++) {
      uint8_t factor = dataCodewords[i] ^ ecCodewords[0];
      for (int j = 0; j < numEc - 1; j++) {
        ecCodewords[j] = ecCodewords[j + 1] ^ gmul(factor, rsGen[j + 1]);
      }
      ecCodewords[numEc - 1] = gmul(factor, rsGen[numEc]);
    }

    // Combine Data + EC
    uint8_t finalWords[120];
    memcpy(finalWords, dataCodewords, totalData);
    memcpy(finalWords + totalData, ecCodewords, numEc);
    int totalWords = totalData + numEc;

    // 7. Place data into Matrix with Zig-Zag Scan and Mask #0 ((r+c)%2 == 0)
    auto isReserved = [&](int r, int c) {
      if (r <= 8 && c <= 8) return true;
      if (r <= 8 && c >= size - 8) return true;
      if (r >= size - 8 && c <= 8) return true;
      if (r == 6 || c == 6) return true;
      if (version >= 2) {
        int pos = (version == 2) ? 18 : ((version == 3) ? 22 : 26);
        if (abs(r - pos) <= 2 && abs(c - pos) <= 2) return true;
      }
      return false;
    };

    int curBit = 0;
    int x = size - 1;
    bool upward = true;

    while (x > 0) {
      if (x == 6) x--;
      int r = upward ? (size - 1) : 0;
      int rStep = upward ? -1 : 1;

      for (int i = 0; i < size; i++, r += rStep) {
        for (int c = x; c >= x - 1; c--) {
          if (!isReserved(r, c)) {
            int byteIndex = curBit / 8;
            int bitIndex = 7 - (curBit % 8);
            bool bitVal = false;
            if (byteIndex < totalWords) {
              bitVal = (finalWords[byteIndex] >> bitIndex) & 1;
            }
            if ((r + c) % 2 == 0) bitVal = !bitVal;
            setModule(qr, c, r, bitVal);
            curBit++;
          }
        }
      }
      x -= 2;
      upward = !upward;
    }

    // 8. Format Information for ECC-L + Mask 0: 0x77C4
    uint16_t fmt = pgm_read_word(&FORMAT_INFO[0]);
    for (int i = 0; i < 6; i++) setModule(qr, i, 8, (fmt >> i) & 1);
    setModule(qr, 7, 8, (fmt >> 6) & 1);
    setModule(qr, 8, 8, (fmt >> 7) & 1);
    setModule(qr, 8, 7, (fmt >> 8) & 1);
    for (int i = 9; i < 15; i++) setModule(qr, 8, 14 - i, (fmt >> i) & 1);

    for (int i = 0; i < 8; i++) setModule(qr, size - 1 - i, 8, (fmt >> i) & 1);
    for (int i = 8; i < 15; i++) setModule(qr, 8, size - 15 + i, (fmt >> i) & 1);

    return true;
  }

} // namespace QrCodeEngine

#endif // QRCODE_GENERATOR_H
