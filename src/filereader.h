#ifndef FILE_READER_H
#define FILE_READER_H


#include "reader.h"

typedef struct {
  const char *filename;
  BitMode mode;
} FileConfig;

Reader *GetFileReader(const char *filename, BitMode mode);
int FileReaderHasError(void);
unsigned char FileReaderErrorSite(void);
unsigned char FileReaderErrorTrack(void);
unsigned char FileReaderErrorSector(void);
void GetFileInfo(unsigned char *track, unsigned char *sector);
void GetFileReadStatus(unsigned char *successes, unsigned char *errors,
                       unsigned char *lastError);
#endif
