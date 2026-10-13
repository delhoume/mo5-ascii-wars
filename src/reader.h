#ifndef READER_H
#define READER_H

typedef void (*ReaderInitFn)(void *);
typedef void (*ReaderResetFn)();
typedef int (*ReaderEofFn)();
typedef unsigned char (*ReaderNextByteFn)();
typedef void (*ReaderCloseFn)();

typedef struct {
  ReaderInitFn init;
  ReaderResetFn reset;
  ReaderEofFn eof;
  ReaderNextByteFn nextByte;
  ReaderCloseFn close;
} Reader;

typedef enum  {
  MODE_7BIT = 0,
  MODE_8BIT = 1
} BitMode ; 

Reader* GetReader();
void SetReader(Reader* reader);
void SetReaderMode(BitMode mode);
int ReaderHasError();

unsigned char GetNextValue();

#endif
