#include "filereader.h"

unsigned char findFileSector(const char *targetName);
extern unsigned char sectorBuffer[256];

unsigned char fileName[12];
unsigned char fileNameValid;
static unsigned char firstFileBlock;
static unsigned char currentBlock;
static unsigned char currentBlockLoaded;
static unsigned char currentBlockIsLast;
static unsigned char nextBlock;
static unsigned char sectorsInCurrentBlock;
static unsigned char sectorInCurrentBlock;
static unsigned char blocksLoaded;
static unsigned char sectorLoaded;
static unsigned char sectorBytePosition;
static unsigned char bytesInCurrentSector;
static unsigned char fileReadError;
static unsigned char fileErrorSite;
unsigned char fileErrorTrack;
unsigned char fileErrorSector;
static unsigned char fileReadEof;
static BitMode fileReaderMode;

static void InitFileReader(void *config) {
  FileConfig *fconfig = (FileConfig *)config;
  const char *inputName = fconfig->filename;
  unsigned char inputLength = 0;
  unsigned char dotPosition = 0;
  unsigned char baseLength;
  unsigned char extensionLength;
  unsigned char extension[3];

  fileNameValid = 0;
  if (inputName == 0) {
    return;
  }

  while (inputName[inputLength] != '\0' && inputLength < sizeof(fileName)) {
    fileName[inputLength] = (unsigned char)inputName[inputLength];
    inputLength++;
  }
  if (inputLength >= sizeof(fileName)) {
    return;
  }
  fileName[inputLength] = '\0';

  for (unsigned char i = 0; i < inputLength; i++) {
    if (fileName[i] >= 'a' && fileName[i] <= 'z') {
      fileName[i] = (unsigned char)(fileName[i] - ('a' - 'A'));
    }
    if (fileName[i] == '.') {
      if (dotPosition != 0) {
        return;
      }
      dotPosition = i + 1;
    }
  }
  if (dotPosition == 0) {
    return;
  }

  baseLength = dotPosition - 1;
  extensionLength = inputLength - dotPosition;
  if (baseLength == 0 || baseLength > 8 || extensionLength == 0 ||
      extensionLength > 3) {
    return;
  }

  for (unsigned char i = 0; i < extensionLength; i++) {
    extension[i] = fileName[dotPosition + i];
  }
  for (unsigned char i = baseLength; i < 8; i++) {
    fileName[i] = ' ';
  }
  for (unsigned char i = 0; i < extensionLength; i++) {
    fileName[8 + i] = extension[i];
  }
  for (unsigned char i = extensionLength; i < 3; i++) {
    fileName[8 + i] = ' ';
  }
  fileName[11] = '\0';
  fileReaderMode = fconfig->mode;
  SetReaderMode(fileReaderMode);
  if (!findFileSector((const char *)fileName)) {
    return;
  }
  fileNameValid = 1;
  currentBlock = firstFileBlock;
  currentBlockLoaded = 0;
  currentBlockIsLast = 0;
  nextBlock = 0;
  sectorsInCurrentBlock = 0;
  sectorInCurrentBlock = 0;
  blocksLoaded = 0;
  sectorLoaded = 0;
  sectorBytePosition = 0;
  bytesInCurrentSector = 0;
  fileReadError = 0;
  fileReadEof = 0;
}

static int loadNextSector(void);

static void resetFileReader(void) {
  currentBlock = firstFileBlock;
  currentBlockLoaded = 0;
  currentBlockIsLast = 0;
  nextBlock = 0;
  sectorsInCurrentBlock = 0;
  sectorInCurrentBlock = 0;
  blocksLoaded = 0;
  sectorLoaded = 0;
  sectorBytePosition = 0;
  bytesInCurrentSector = 0;
  fileReadError = 0;
  fileReadEof = 0;
  SetReaderMode(fileReaderMode);
}

static int eofFileReader(void) {
  if (fileReadError || fileReadEof) {
    return 1;
  }
  if (!sectorLoaded || sectorBytePosition >= bytesInCurrentSector) {
    return !loadNextSector();
  }
  return 0;
}

static unsigned char nextByteFileReader(void) {
  if (eofFileReader()) {
    return 0;
  }
  return sectorBuffer[sectorBytePosition++];
}

static void closeFileReader() {}

static Reader FileReader = {
    InitFileReader,     resetFileReader, eofFileReader,
    nextByteFileReader, closeFileReader,
};

Reader *GetFileReader(const char *filename, BitMode mode) {
  FileConfig config = {filename, mode};
  FileReader.init(&config);
  if (!fileNameValid) {
    return 0;
  }
  return &FileReader;
}

int FileReaderHasError(void) { return fileReadError; }
unsigned char FileReaderErrorSite(void) { return fileErrorSite; }
unsigned char FileReaderErrorTrack(void) { return fileErrorTrack; }
unsigned char FileReaderErrorSector(void) { return fileErrorSector; }

// Allocate a standard 256-byte sector reading buffer in RAM
unsigned char sectorBuffer[256];

// Global results found by our locator function
unsigned char fileStartTrack = 0;
unsigned char fileStartSector = 0;
static unsigned int fileFinalByteCount = 0;
unsigned char directoryReadSuccesses = 0;
unsigned char directoryReadErrors = 0;
unsigned char lastDirectoryReadError = 0;

void GetFileInfo(unsigned char *track, unsigned char *sector) {
  *track = fileStartTrack;
  *sector = fileStartSector;
}

void GetFileReadStatus(unsigned char *successes, unsigned char *errors,
                       unsigned char *lastError) {
  *successes = directoryReadSuccesses;
  *errors = directoryReadErrors;
  *lastError = lastDirectoryReadError;
}

/* Read one MO sector through the monitor's disk parameter block and SWI. */
unsigned char readThomsonSector(unsigned char drive, unsigned char track,
                                unsigned char sector, void *buffer) {
  unsigned char errorFlag = 0;

  asm {
    PSHS  D,X,Y,U,DP
    LDA   $A7C0           ; Preserve the selected MO5 form/color plane
    PSHS  A
    LDA   #$20
    TFR   A,DP             ; MO monitor disk variables are in page $20
    LDA   :drive
    STA   $2049           ; DK.DRV
    CLR   $204A           ; DK.TRK high byte
    LDA   :track
    STA   $204B           ; DK.TRK low byte
    LDA   :sector
    STA   $204C           ; DK.SEC
    LDX   :buffer
    STX   $204F           ; DK.BUF
    LDA   #$02            ; Read command
    STA   $2048           ; DK.OPC
    SWI
    FCB   $26             ; MO disk service
    BCC   .ok
    LDA   $204E           ; DK.STA: disk error code
    STA   :errorFlag
.ok:
    PULS  A
    STA   $A7C0
    PULS  D,X,Y,U,DP
  }

  return errorFlag;
}

static int loadNextSector(void) {
  unsigned char fatEntry;
  unsigned char track;
  unsigned char sector;
  unsigned char error;

  sectorLoaded = 0;
  if (fileReadError || fileReadEof) {
    return 0;
  }

  for (;;) {
    if (!currentBlockLoaded) {
      error = readThomsonSector(0, 20, 2, sectorBuffer);
      if (error != 0) {
        directoryReadErrors++;
        lastDirectoryReadError = error;
        fileReadError = 1; fileErrorSite = 1;
        return 0;
      }

      if (++blocksLoaded > 160) {
        fileReadError = 1; fileErrorSite = 2;
        return 0;
      }
      fatEntry = sectorBuffer[currentBlock + 1];
      if (fatEntry >= 0xC1 && fatEntry <= 0xC8) {
        currentBlockIsLast = 1;
        sectorsInCurrentBlock = fatEntry - 0xC0;
        nextBlock = 0xFF;
      } else if (fatEntry < 160) {
        currentBlockIsLast = 0;
        sectorsInCurrentBlock = 8;
        nextBlock = fatEntry;
      } else {
        fileReadError = 1; fileErrorSite = 3;
        return 0;
      }
      sectorInCurrentBlock = 0;
      currentBlockLoaded = 1;
    }

    if (sectorInCurrentBlock >= sectorsInCurrentBlock) {
      if (currentBlockIsLast) {
        fileReadEof = 1;
        return 0;
      }
      currentBlock = nextBlock;
      currentBlockLoaded = 0;
      continue;
    }

    track = currentBlock / 2;
    sector = (currentBlock & 1) ? 9 : 1;
    sector += sectorInCurrentBlock;
    error = readThomsonSector(0, track, sector, sectorBuffer);
    for (unsigned char retry = 0; error != 0 && retry < 3; retry++) {
      // re-seek the head on a distant track, then retry the real sector
      readThomsonSector(0, track < 40 ? 70 - track : 0, 1, sectorBuffer);
      error = readThomsonSector(0, track, sector, sectorBuffer);
    }
    if (error != 0) {
      fileErrorTrack = track;
      fileErrorSector = sector;
      directoryReadErrors++;
      lastDirectoryReadError = error;
      fileReadError = 1; fileErrorSite = 4;
      return 0;
    }
    directoryReadSuccesses++;

    bytesInCurrentSector = 255;
    if (currentBlockIsLast &&
        sectorInCurrentBlock == sectorsInCurrentBlock - 1) {
      bytesInCurrentSector = (unsigned char)fileFinalByteCount;
    }
    if (bytesInCurrentSector == 0) {
      fileReadError = 1; fileErrorSite = 5;
      return 0;
    }
    sectorBytePosition = 0;
    sectorLoaded = 1;
    sectorInCurrentBlock++;
    return 1;
  }
}

/*
 * Scan the fdfs directory on Track 20 and locate an 11-character
 * space-padded filename/extension such as "ANIM7   RAW".
 */
unsigned char findFileSector(const char *targetName) {
  unsigned char currentSector;
  unsigned char entryIndex;
  unsigned char charIndex;
  unsigned int entryOffset;

  fileStartTrack = 0;
  fileStartSector = 0;
  directoryReadSuccesses = 0;
  directoryReadErrors = 0;
  lastDirectoryReadError = 0;

  // The fdfs directory occupies Track 20, Sectors 3 through 16.
  for (currentSector = 3; currentSector <= 16; currentSector++) {
    unsigned char error =
        readThomsonSector(0, 20, currentSector, sectorBuffer);
    if (error != 0) {
      directoryReadErrors++;
      lastDirectoryReadError = error;
      continue;
    }
    directoryReadSuccesses++;

    for (entryIndex = 0; entryIndex < 8; entryIndex++) {
      entryOffset = entryIndex * 32;

      // fdfs stores the first filename character at byte 0; 0 and 0xFF
      // mark deleted and unused entries, respectively.
      if (sectorBuffer[entryOffset] <= 0x20 ||
          sectorBuffer[entryOffset] >= 0xFE) {
        continue;
      }

      // The 8-byte filename and 3-byte extension start at byte 0.
      for (charIndex = 0; charIndex < 11; charIndex++) {
        if (sectorBuffer[entryOffset + charIndex] != targetName[charIndex]) {
          break;
        }
      }
      if (charIndex == 11) {
        unsigned char firstBlock = sectorBuffer[entryOffset + 13];

        // The directory stores a block number, not a track/sector pair.
        // Each block is 8 sectors: block N starts at T(N/2), S1 or S9.
        if (firstBlock >= 160) {
          return 0;
        }
        firstFileBlock = firstBlock;
        fileFinalByteCount =
            ((unsigned int)sectorBuffer[entryOffset + 14] << 8) |
            sectorBuffer[entryOffset + 15];
        if (fileFinalByteCount == 0 || fileFinalByteCount > 255) {
          return 0;
        }
        fileStartTrack = firstBlock / 2;
        fileStartSector = (firstBlock & 1) ? 9 : 1;
        return 1;
      }
    }
  }
  return 0;
}
