#ifndef MEMORY_READER_H
#define MEMORY_READER_H

#include "reader.h"

typedef struct {
  const unsigned char *input;
  unsigned int size;
  BitMode mode;
} MemoryConfig;

Reader* GetMemoryReader(const unsigned char* ptr, unsigned int size, BitMode mode);
#endif
