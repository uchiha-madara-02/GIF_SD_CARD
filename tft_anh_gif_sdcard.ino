#include <SD.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <AnimatedGIF.h>
#include <vector>

// --- Chân kết nối SD card ---
// #define SD_CS_PIN   1   
// #define SD_SCK_PIN  3
// #define SD_MISO_PIN 4
// #define SD_MOSI_PIN 2

#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_SCK_PIN  18
#define SD_CS_PIN   5

// SPI bus cho SD riêng
SPIClass SD_SPI(VSPI);

// Đối tượng màn hình và GIF
TFT_eSPI tft = TFT_eSPI();
AnimatedGIF gif;
File sdFile;

struct GifEntry {
  String filename;
  int x;
  int y;
  int rotation;   // 0..3 tương ứng với các giá trị của tft.setRotation()
};
std::vector<GifEntry> gifList;  // Danh sách ảnh và vị trí

// Buffer cho GIFDraw
#define BUFFER_SIZE (240 * 280 * 2 / 4)  // >= độ rộng GIF hoặc chia nguyên
#ifdef USE_DMA
static uint16_t usTemp[2][BUFFER_SIZE];
#else
static uint16_t usTemp[1][BUFFER_SIZE];
#endif
static bool dmaBuf = 0;

int X_OFFSET = 0;  // dịch phải 20 pixel
int Y_OFFSET = 0;  // dịch xuống 30 pixel

int currentIndex = 0;

void loadMetadata(const char* metaFile = "/metadata.txt") {
  File file = SD.open(metaFile);
  if (!file) {
    Serial.println("Không tìm thấy metadata.txt");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line.startsWith("#")) continue;

    // Tách ra 4 phần bởi dấu phẩy
    int comma1 = line.indexOf(',');
    int comma2 = line.indexOf(',', comma1 + 1);
    int comma3 = line.indexOf(',', comma2 + 1);

    if (comma1 < 0 || comma2 < 0 || comma3 < 0) continue;

    String name = line.substring(0, comma1);
    int x = line.substring(comma1 + 1, comma2).toInt();
    int y = line.substring(comma2 + 1, comma3).toInt();
    int rot = line.substring(comma3 + 1).toInt();

    gifList.push_back({ name, x, y, rot });
    Serial.printf("Đã tải: %s (%d, %d), rot=%d\n", name.c_str(), x, y, rot);
  }

  file.close();
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Khởi tạo màn hình TFT
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  // Khởi tạo SD card trên bus riêng
  Serial.println("Initializing SD card...");
  SD_SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, SD_SPI)) {
    Serial.println("SD begin failed!");
    while (true) { delay(1000); }
  }
  Serial.println("SD initialized.");

  // Khởi tạo GIF library
  gif.begin(BIG_ENDIAN_PIXELS);  // hoặc LITTLE_ENDIAN_PIXELS tuỳ màn
  loadMetadata();  // Tải danh sách ảnh từ file

  Serial.println("Ready to play GIF.");
}

void loop() {
  if (gifList.empty()) {
    Serial.println("Không có ảnh nào trong metadata.txt");
    delay(3000);
    return;
  }

  GifEntry& entry = gifList[currentIndex];
  Serial.printf("Phát %s tại (%d, %d), rot=%d\n", entry.filename.c_str(), entry.x, entry.y, entry.rotation);

  // Áp dụng rotation
  tft.setRotation(entry.rotation);

  if (gif.open(entry.filename.c_str(), openSD, closeSD, readSD, seekSD, GIFDraw)) {
    X_OFFSET = entry.x;
    Y_OFFSET = entry.y;
    tft.fillScreen(TFT_BLACK);

    tft.startWrite();
    while (gif.playFrame(true, NULL)) { }
    tft.endWrite();
    gif.close();
    Serial.println("Xong ảnh.");
  } else {
    Serial.printf("Không mở được file: %s\n", entry.filename.c_str());
  }

  currentIndex = (currentIndex + 1) % gifList.size();
  delay(50);
}


// Callbacks file I/O từ SD
void* openSD(const char* name, int32_t* pFileSize) {
  sdFile = SD.open(name);
  if (!sdFile) {
    Serial.println("Cannot open file on SD.");
    return nullptr;
  }
  *pFileSize = sdFile.size();
  yield();
  return &sdFile;
}

void closeSD(void* pHandle) {
  sdFile.close();
}

int32_t readSD(GIFFILE* pFile, uint8_t* pBuf, int32_t iLen) {
  int32_t toRead = min(iLen, pFile->iSize - pFile->iPos);
  if (toRead <= 0) return 0;
  sdFile.seek(pFile->iPos);
  int32_t r = sdFile.read(pBuf, toRead);
  pFile->iPos += toRead;
  yield();
  return r;
}

int32_t seekSD(GIFFILE* pFile, int32_t iPosition) {
  iPosition = constrain(iPosition, 0, pFile->iSize - 1);
  pFile->iPos = iPosition;
  sdFile.seek(pFile->iPos);
  yield();
  return iPosition;
}

// Vẽ frame từ AnimatedGIF
void GIFDraw(GIFDRAW *pDraw) {
  int x = pDraw->iX + X_OFFSET;  
  int y = pDraw->iY + pDraw->y + Y_OFFSET;
  int iWidth = pDraw->iWidth;
  if (iWidth + pDraw->iX > tft.width()) iWidth = tft.width() - pDraw->iX;
  uint8_t *src = pDraw->pPixels;
  uint16_t *usPalette = pDraw->pPalette;

  // Xử lý disposal
  if (pDraw->ucDisposalMethod == 2) {
    for (int i = 0; i < iWidth; i++) {
      if (src[i] == pDraw->ucTransparent) src[i] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }

  if (pDraw->ucHasTransparency) {
    // Chạy qua từng pixel, gom non-transp rồi push
    int i = 0;
    while (i < iWidth) {
      // tìm đoạn opaque
      int len = 0;
      while (i+len < iWidth && src[i+len] != pDraw->ucTransparent && len < BUFFER_SIZE) {
        usTemp[0][len] = usPalette[src[i+len]];
        len++;
      }
      if (len) {
        tft.setAddrWindow(x + i, y, len, 1);
        tft.pushPixels(usTemp[0], len);
        i += len;
      }
      // skip transparent
      while (i < iWidth && src[i] == pDraw->ucTransparent) i++;
    }
  } else {
    // Không transparency: buffer toàn bộ line
    int count = min(iWidth, BUFFER_SIZE);
    for (int i = 0; i < count; i++) usTemp[dmaBuf][i] = usPalette[src[i]];
#ifdef USE_DMA
    tft.dmaWait();
    tft.setAddrWindow(x, y, count, 1);
    tft.pushPixelsDMA(usTemp[dmaBuf], count);
    dmaBuf = !dmaBuf;
#else
    tft.setAddrWindow(x, y, count, 1);
    tft.pushPixels(usTemp[0], count);
#endif
    iWidth -= count;
    int offset = count;
    while (iWidth > 0) {
      count = min(iWidth, BUFFER_SIZE);
      for (int i = 0; i < count; i++) usTemp[dmaBuf][i] = usPalette[src[offset + i]];
#ifdef USE_DMA
      tft.dmaWait();
      tft.pushPixelsDMA(usTemp[dmaBuf], count);
      dmaBuf = !dmaBuf;
#else
      tft.pushPixels(usTemp[0], count);
#endif
      offset += count;
      iWidth -= count;
    }
  }
}
