#ifndef SD_CARD_H
#define SD_CARD_H

#include "FS.h"
#include "SD.h"
#include "SPI.h"

#define SD_CS   40
#define SD_MOSI 42
#define SD_MISO 41
#define SD_SCLK 44

class SdCard {
   private:
    char buf[128];

   public:
    void init();

    void listDir(const char* dirname, uint8_t levels);

    void createDir(const char* path);

    void removeDir(const char* path);

    void readFile(const char* path);

    String readFileLine(const char* path, int num);

    void writeFile(const char* path, const char* message);

    void appendFile(const char* path, const char* message);

    void renameFile(const char* path1, const char* path2);

    void deleteFile(const char* path);

    void readBinFromSd(const char* path, uint8_t* buf);

    void writeBinToSd(const char* path, uint8_t* buf);

    void fileIO(const char* path);

    bool File_Search(const char* directory, const char* fileName);
    uint16_t Folder_retrieval(const char* directory, const char* fileExtension,
                              char File_Name[][100], uint16_t maxFiles);
};

extern SdCard tf;

#endif