#include "reader.h"


static Reader* reader = 0;
static unsigned char readerError;

BitMode bitmode = MODE_7BIT;

static unsigned int value7_accumulator;
static unsigned char value7_bits;

Reader* GetReader(void) {
  return reader;
}

void SetReader(Reader* areader) {
  if (reader) reader->close();
  reader = areader;
  readerError = 0;
  value7_accumulator = 0;
  value7_bits = 0;
}

void SetReaderMode(BitMode mode) {
  bitmode = mode;
  readerError = 0;
  value7_accumulator = 0;
  value7_bits = 0;
}

static unsigned char get_next_value7(void) {
  if (reader == 0) {
    readerError = 1;
    return 0;
  }

  while (value7_bits < 7) {
    if (reader->eof()) {
      readerError = 1;
      return 0;
    }

    value7_accumulator |= (unsigned int)reader->nextByte() << value7_bits;
    value7_bits += 8;
  }

  unsigned char value = (unsigned char)(value7_accumulator & 0x7f);
  value7_accumulator >>= 7;
  value7_bits -= 7;
  return value;
}

static unsigned char get_next_value8(void) {
  if (reader == 0 || reader->eof()) {
    readerError = 1;
    return 0;
  }
  return reader->nextByte();
}

unsigned char GetNextValue(void) {
  return bitmode == MODE_7BIT ? get_next_value7() : get_next_value8();
}

int ReaderHasError(void) {
  return readerError;
}
