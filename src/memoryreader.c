#include "memoryreader.h"
#include "reader.h"

static const unsigned char *sptr;
static const unsigned char *fptr;
static const unsigned char *fend;
static unsigned int value7_accumulator;
static unsigned char value7_bits;
static BitMode readerMode;

static void InitMemoryReader(void *config) {
  MemoryConfig *mconfig = (MemoryConfig *)config;
  sptr = mconfig->input;
  fptr = mconfig->input;
  fend = fptr + mconfig->size;
  value7_accumulator = 0;
  value7_bits = 0;
  readerMode = mconfig->mode;
  SetReaderMode(readerMode);
}


static void resetMemoryReader(void) {
  fptr = sptr;
  SetReaderMode(readerMode);
}

static int eofMemoryReader(void) {
  return fptr >= fend;
}

static unsigned char nextByteMemoryReader(void) {
  return *fptr++;
}

static void closeMemoryReader(void) {}

static Reader MemoryReader = {
    InitMemoryReader,
    resetMemoryReader,
    eofMemoryReader,
    nextByteMemoryReader,
    closeMemoryReader,
};

 Reader* GetMemoryReader(const unsigned char* ptr, unsigned int size, BitMode mode) {
    MemoryConfig config = { ptr, size, mode };
   MemoryReader.init(&config);
   return &MemoryReader;
 }
