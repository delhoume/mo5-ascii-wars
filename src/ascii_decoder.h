#ifndef ASCII_DECODER_H
#define ASCII_DECODER_H

static const unsigned char *fptr;
static const unsigned char *fend;
static unsigned int value7_accumulator;
static unsigned char value7_bits;
static unsigned char input_value_bits;
static unsigned char FRAME[FRAME_CHAR_HEIGHT][FRAME_CHAR_WIDTH + 1];
static unsigned char LINELEN[FRAME_CHAR_HEIGHT];
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

static int get_next_frame(void) {
  if (FRAME_DECODE_ERROR)
    return 0;

  unsigned char row = 0;
  unsigned char line_length = 0;

  while (row < FRAME_CHAR_HEIGHT) {
    unsigned char c = GET_NEXT_VALUE();
    if (FRAME_DECODE_ERROR)
      return 0;
    if (c == 0) {
      if (row != 0 || line_length != 0)
        FRAME_DECODE_ERROR = 1;
      return 0;
    }

    if (c == '\n') {
      FRAME[row][line_length] = 0;
      LINELEN[row] = line_length;
      row++;
      line_length = 0;
    } else {
      unsigned char decoded =
          decode(c, FRAME[row] + line_length,
                 FRAME_CHAR_WIDTH - line_length);
      if (decoded == 0 || FRAME_DECODE_ERROR) {
        FRAME_DECODE_ERROR = 1;
        return 0;
      }
      line_length += decoded;
    }
  }

  return 1;
}

#endif
