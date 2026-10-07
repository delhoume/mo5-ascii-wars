#ifndef ASCII_DECODER_H
#define ASCII_DECODER_H

int drawCharOptimised(int x, int y, int c);
int drawChar(int x, int y, int c);

static const unsigned char *fptr;
static const unsigned char *fend;
static unsigned int value7_accumulator;
static unsigned char value7_bits;
static unsigned char input_value_bits;
static unsigned char FRAME_DECODE_ERROR;

static void RESET_INPUT(const unsigned char *input, unsigned int input_size,
                        unsigned char value_bits) {
  fptr = input;
  fend = input + input_size;
  value7_accumulator = 0;
  value7_bits = 0;
  input_value_bits = value_bits;
  FRAME_DECODE_ERROR = value_bits != 7 && value_bits != 8;
}

static unsigned char get_next_value7(void) {
  while (value7_bits < 7) {
    if (fptr >= fend) {
      FRAME_DECODE_ERROR = 1;
      return 0;
    }
    value7_accumulator |= (unsigned int)(*fptr++) << value7_bits;
    value7_bits += 8;
  }

  unsigned char value = (unsigned char)(value7_accumulator & 0x7f);
  value7_accumulator >>= 7;
  value7_bits -= 7;
  return value;
}

static unsigned char get_next_value8(void) {
  if (fptr >= fend) {
    FRAME_DECODE_ERROR = 1;
    return 0;
  }
  return *fptr++;
}

static unsigned char GET_NEXT_VALUE(void) {
  return input_value_bits == 7 ? get_next_value7() : get_next_value8();
}

static unsigned char decode(unsigned char c, unsigned char *ptr,
                            unsigned char available) {
  if (c == 3) {
    unsigned char repeat = GET_NEXT_VALUE();
    c = GET_NEXT_VALUE();
    if (repeat == 0 || repeat > available)
      return 0;
    memset(ptr, c, repeat);
    return repeat;
  } else if (c >= 4 && c <= 8) {
    if (available < 2)
      return 0;
    static const unsigned char val[5] = {95, 92, 45, 124, 32};
    c = val[8 - c];
    ptr[0] = c;
    ptr[1] = c;
    return 2;
  } else if (c == 9) {
    if (available < 3)
      return 0;
    ptr[0] = ' ';
    ptr[1] = ' ';
    ptr[2] = ' ';
    return 3;
  } else if (c >= 11 && c <= 23) {
    unsigned char repeat = c - 7;
    if (repeat > available)
      return 0;
    memset(ptr, ' ', repeat);
    return repeat;
  }

  if (available == 0)
    return 0;
  ptr[0] = c;
  return 1;
}


 static int draw_rle_opcode(int *x, int y, unsigned char op) {
  switch (op) {
    case 3: {
      unsigned char repeat = GET_NEXT_VALUE();
      unsigned char ch = GET_NEXT_VALUE();
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
      static const unsigned char val[5] = {95, 92, 45, 124, 32};
      unsigned char ch = val[8 - op];
      *x += drawCharOptimised(*x, y, ch) + ADVANCEX;
      *x += drawCharOptimised(*x, y, ch) + ADVANCEX;
      return 1;
    }

    case 9: {
      *x +=  3 * TWIDTH;
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

static int get_next_frame(void) {
  if (FRAME_DECODE_ERROR)
    return 0;

  int x = FRAME_START_X;
  int y = FRAME_START_Y;
  unsigned char newline_count = 0;
  while (1) {
    unsigned char op = GET_NEXT_VALUE();
    if (FRAME_DECODE_ERROR)
      return 0;

    if (op == 0)
      return 1;

    if (op == '\n') {
      x = FRAME_START_X;
      y += THEIGHT;
      newline_count++; 
      if (newline_count == FRAME_CHAR_HEIGHT)
        return 1;
  } else {
  int status = draw_rle_opcode(&x, y, op);
    if (status == 0) {
      FRAME_DECODE_ERROR = 1;
      return 0;
    }
  }
}
}
#endif
