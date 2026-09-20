#include "sd_card.h"


void SdCard::init()
{
    SPIClass* SD_SPI = new SPIClass(HSPI); 
	 // SPI.begin(sck, miso, mosi, cs);
        // SPI1.begin(44, 41, 42, 40);
    SD_SPI->begin(SD_SCLK, SD_MISO, SD_MOSI,-1); // SD_SPI.setFrequency(1000000);
 
	// if (!SD.begin(SD_CS, *SD_SPI, 1000000)) // SD-Card SS pin is 15
	// {
	// 	Serial.println("Card Mount Failed");
	// 	return;
	// }

    while (1)
    {
        // SPI.begin(sck, miso, mosi, cs);
        // SPI1.begin(44, 41, 42, 40);  

        if (SD.begin(SD_CS, *SD_SPI, 4000000))
        {
            printf("initialization done.");
            break;
        }
        printf("initialization failed!");
    }

	uint8_t cardType = SD.cardType();

	if (cardType == CARD_NONE)
	{
		Serial.println("No SD card attached");
		return;
	}

	Serial.print("SD Card Type: ");
	if (cardType == CARD_MMC)
	{
		Serial.println("MMC");
	}
	else if (cardType == CARD_SD)
	{
		Serial.println("SDSC");
	}
	else if (cardType == CARD_SDHC)
	{
		Serial.println("SDHC");
	}
	else
	{
		Serial.println("UNKNOWN");
	}

	uint64_t cardSize = SD.cardSize() / (1024 * 1024);
	Serial.printf("SD Card Size: %lluMB\n", cardSize);
}



void SdCard::listDir(const char* dirname, uint8_t levels)
{
	Serial.printf("Listing directory: %s\n", dirname);

	File root = SD.open(dirname);
	if (!root)
	{
		Serial.println("Failed to open directory");
		return;
	}
	if (!root.isDirectory())
	{
		Serial.println("Not a directory");
		return;
	}

	File file = root.openNextFile();
	while (file)
	{
		if (file.isDirectory())
		{
			Serial.print("  DIR : ");
			Serial.println(file.name());
			if (levels)
			{
				listDir(file.name(), levels - 1);
			}
		}
		else
		{
			Serial.print("  FILE: ");
			Serial.print(file.name());
			Serial.print("  SIZE: ");
			Serial.println(file.size());
		}
		file = root.openNextFile();
	}
}

void SdCard::createDir(const char* path)
{
	Serial.printf("Creating Dir: %s\n", path);
	if (SD.mkdir(path))
	{
		Serial.println("Dir created");
	}
	else
	{
		Serial.println("mkdir failed");
	}
}

void SdCard::removeDir(const char* path)
{
	Serial.printf("Removing Dir: %s\n", path);
	if (SD.rmdir(path))
	{
		Serial.println("Dir removed");
	}
	else
	{
		Serial.println("rmdir failed");
	}
}

void SdCard::readFile(const char* path)
{
	Serial.printf("Reading file: %s\n", path);

	File file = SD.open(path);
	if (!file)
	{
		Serial.println("Failed to open file for reading");
		return;
	}

	Serial.print("Read from file: ");
	while (file.available())
	{
		Serial.write(file.read());
	}
	file.close();
}

String SdCard::readFileLine(const char* path, int num = 1)
{
	Serial.printf("Reading file: %s line: %d\n", path, num);

	File file = SD.open(path);
	if (!file)
	{
		return ("Failed to open file for reading");
	}

	char* p = buf;
	while (file.available())
	{
		char c = file.read();
		if (c == '\n')
		{
			num--;
			if (num == 0)
			{
				*(p++) = '\0';
				String s(buf);
				s.trim();
				return s;
			}
		}
		else if (num == 1)
		{
			*(p++) = c;
		}
	}
	file.close();

	return  String("error parameter!");
}

void SdCard::writeFile(const char* path, const char* message)
{
	Serial.printf("Writing file: %s\n", path);

	File file = SD.open(path, FILE_WRITE);
	if (!file)
	{
		Serial.println("Failed to open file for writing");
		return;
	}
	if (file.print(message))
	{
		Serial.println("File written");
	}
	else
	{
		Serial.println("Write failed");
	}
	file.close();
}

void SdCard::appendFile(const char* path, const char* message)
{
	Serial.printf("Appending to file: %s\n", path);

	File file = SD.open(path, FILE_APPEND);
	if (!file)
	{
		Serial.println("Failed to open file for appending");
		return;
	}
	if (file.print(message))
	{
		Serial.println("Message appended");
	}
	else
	{
		Serial.println("Append failed");
	}
	file.close();
}

void SdCard::renameFile(const char* path1, const char* path2)
{
	Serial.printf("Renaming file %s to %s\n", path1, path2);
	if (SD.rename(path1, path2))
	{
		Serial.println("File renamed");
	}
	else
	{
		Serial.println("Rename failed");
	}
}

void SdCard::deleteFile(const char* path)
{
	Serial.printf("Deleting file: %s\n", path);
	if (SD.remove(path))
	{
		Serial.println("File deleted");
	}
	else
	{
		Serial.println("Delete failed");
	}
}

void SdCard::readBinFromSd(const char* path, uint8_t* buf)
{
	File file = SD.open(path);
	size_t len = 0;
	if (file)
	{
		len = file.size();
		size_t flen = len;

		while (len)
		{
			size_t toRead = len;
			if (toRead > 512)
			{
				toRead = 512;
			}
			file.read(buf, toRead);
			len -= toRead;
		}

		file.close();
	}
	else
	{
		Serial.println("Failed to open file for reading");
	}
}

void SdCard::writeBinToSd(const char* path, uint8_t* buf)
{
	File file = SD.open(path, FILE_WRITE);
	if (!file)
	{
		Serial.println("Failed to open file for writing");
		return;
	}

	size_t i;
	for (i = 0; i < 2048; i++)
	{
		file.write(buf, 512);
	}
	file.close();
}


void SdCard::fileIO(const char* path)
{
	File file = SD.open(path);
	static uint8_t buf[512];
	size_t len = 0;
	uint32_t start = millis();
	uint32_t end = start;
	if (file)
	{
		len = file.size();
		size_t flen = len;
		start = millis();
		while (len)
		{
			size_t toRead = len;
			if (toRead > 512)
			{
				toRead = 512;
			}
			file.read(buf, toRead);
			len -= toRead;
		}
		end = millis() - start;
		Serial.printf("%u bytes read for %u ms\n", flen, end);
		file.close();
	}
	else
	{
		Serial.println("Failed to open file for reading");
	}


	file = SD.open(path, FILE_WRITE);
	if (!file)
	{
		Serial.println("Failed to open file for writing");
		return;
	}

	size_t i;
	start = millis();
	for (i = 0; i < 2048; i++)
	{
		file.write(buf, 512);
	}
	end = millis() - start;
	Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
	file.close();
}

bool SdCard::File_Search(const char* directory, const char* fileName)    
{
  File Path = SD.open(directory,FILE_READ);
  if (!Path) {
    printf("Path: <%s> does not exist\r\n",directory);
    return false;
  }
  File file = Path.openNextFile();
  while (file) {
    if (strcmp(file.name(), fileName) == 0) {                           
      if (strcmp(directory, "/") == 0)
        printf("File '%s%s' found in root directory.\r\n",directory,fileName);  
      else
        printf("File '%s/%s' found in root directory.\r\n",directory,fileName); 
      Path.close();                                                     
      return true;                                                     
    }
    file = Path.openNextFile();                                        
  }
  if (strcmp(directory, "/") == 0)
    printf("File '%s%s' not found in root directory.\r\n",directory,fileName);           
  else
    printf("File '%s/%s' not found in root directory.\r\n",directory,fileName);          
  Path.close();                                                         
  return false;                                                         
}
uint16_t SdCard::Folder_retrieval(const char* directory, const char* fileExtension, char File_Name[][100],uint16_t maxFiles)    
{
  File Path = SD.open(directory,"r");
  if (!Path) {
    printf("Path: <%s> does not exist\r\n",directory);
    return false;
  }
  
  uint16_t fileCount = 0;
  char filePath[100];
  File file = Path.openNextFile();
  while (file && fileCount < maxFiles) {
    if (!file.isDirectory() && strstr(file.name(), fileExtension)) {
      strncpy(File_Name[fileCount], file.name(), sizeof(File_Name[fileCount])); 
    //   strncpy(File_Name[fileCount], file.name(), sizeof(file.name())); 
      if (strcmp(directory, "/") == 0) {                                      
        snprintf(filePath, 100, "%s%s", directory, file.name());   
      } else {                                                            
        snprintf(filePath, 100, "%s/%s", directory, file.name());
      }
      printf("File found: %s\r\n", filePath);
      fileCount++;
    }
    file = Path.openNextFile();                                      
  }
  Path.close();                                                         
  if (fileCount > 0) {
    printf("Retrieved %d mp3 files\r\n",fileCount);
    return fileCount;                                                 
  } else {
    printf("No files with extension '%s' found in directory: %s\r\n", fileExtension, directory);
    return 0;                                                         
  }
}