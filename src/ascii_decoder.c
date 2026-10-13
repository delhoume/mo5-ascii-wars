
#include "ascii_decoder.h"
#include "reader.h"

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200
#define FRAME_CHAR_WIDTH 67
#define FRAME_CHAR_HEIGHT 13
#define TWIDTH 5
#define THEIGHT 6
#define ADVANCEX 0

static int START_X = (SCREEN_WIDTH - FRAME_CHAR_WIDTH * TWIDTH) / 2;
static int START_Y = (SCREEN_HEIGHT - FRAME_CHAR_HEIGHT * THEIGHT) / 2;

unsigned int drawCharOptimised(int x, int y, unsigned char c);
unsigned int drawChar(int x, int y, unsigned char c);


static int draw_rle_opcode(int *x, int y, unsigned char op) {
  switch (op) {
  case 3: {
    unsigned char repeat = GetNextValue();
    unsigned char ch = GetNextValue();
    if (ReaderHasError())
      return 0;
    if (ch == ' ') {
      *x += (repeat * TWIDTH);
      return 1;
    }

    for (unsigned char i = 0; i < repeat; i++)
      *x += drawCharOptimised(*x, y, ch) + ADVANCEX;
    return 1;
  }

  case 4:
  case 5:
  case 6:
  case 7:
  case 8: {
    //  underscore bakclash slash pipe space
    static const unsigned char val[5] = {95, 92, 47, 124, 32};
    unsigned char ch = val[8 - op];
    *x += drawCharOptimised(*x, y, ch) + ADVANCEX;
    *x += drawCharOptimised(*x, y, ch) + ADVANCEX;
    return 1;
  }

  case 9: {
    *x += 3 * TWIDTH;
    return 1;
  }

    default:
      if (op >= 11 && op <= 23) {
        unsigned char repeat = op - 7;
        *x += repeat * TWIDTH;
        return 1;
      }

    if (op == '\n')
      return 1;

    if (op == 0)
      return 1;

    *x += drawCharOptimised(*x, y, op) + ADVANCEX;
    return 1;
  }
}

int GetNextFrame(void) {
  int x = START_X;
  int y = START_Y;
  unsigned char newline_count = 0;
  while (1) {
    unsigned char op = GetNextValue();
    if (ReaderHasError())
      return 0;
     if (op == 0) return 1;

    if (op == '\n') {
      x = START_X;
      y += THEIGHT;
      newline_count++;
      if (newline_count == FRAME_CHAR_HEIGHT)
        return 1;
    } else {
      int status = draw_rle_opcode(&x, y, op);
      if (status == 0 || ReaderHasError()) {
         return 0;
      }

    }
  }
}
