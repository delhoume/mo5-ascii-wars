/*
 * Frederic Delhoume 2026
 *
 */

 // cmoc optimized functions

 void *memcpy(void *destination, const void *source, unsigned int count);
void *memset(void *destination, int value, unsigned int count);


typedef unsigned char uint8_t;
#include "asciimation8_75.h"
#include "delays_75.h"

#include "nitram5x5_mini.h"
#include <mo5_audio.h>
#include <mo5_video.h>

#define SCREEN_PIXEL_WIDTH 320
#define SCREEN_PIXEL_HEIGHT 200
#define GWIDTH 5
#define ADVANCEX 1
#define GHEIGHT 5

// = GWIDTH + ADVANCEX;
#define TWIDTH 6
#define THEIGHT 6

#define FRAME_CHAR_WIDTH 67
#define FRAME_CHAR_HEIGHT 13
int FRAME_PIXEL_WIDTH = FRAME_CHAR_WIDTH * TWIDTH;
int FRAME_PIXEL_HEIGHT = FRAME_CHAR_HEIGHT * THEIGHT;
int BAND_PIXEL_HEIGHT = (SCREEN_PIXEL_HEIGHT - FRAME_PIXEL_HEIGHT) / 2;
int FRAME_START_Y = (SCREEN_PIXEL_HEIGHT - FRAME_PIXEL_HEIGHT) / 2;
int FRAME_START_X = (SCREEN_PIXEL_WIDTH - FRAME_PIXEL_WIDTH) / 2;

int DOUBLE_BUFFER_PIXEL_HEIGHT = 78; // FRAME_PIXEL_HEIGHT;
#define DOUBLE_BUFFER_SIZE 3120
unsigned char DOUBLE_BUFFER[DOUBLE_BUFFER_SIZE];
unsigned char *DRAW_TARGET = VRAM;
unsigned int DRAW_TARGET_PIXEL_HEIGHT = SCREEN_PIXEL_HEIGHT;
unsigned int DRAW_TARGET_BYTE_WIDTH = SCREEN_WIDTH_BYTES;
unsigned int DRAW_TARGET_PIXEL_WIDTH = SCREEN_PIXEL_WIDTH;

unsigned int show_info = 1;

void set_vram_target() {
  DRAW_TARGET = VRAM;
  DRAW_TARGET_PIXEL_HEIGHT = SCREEN_PIXEL_HEIGHT;
}

void set_double_buffer_target() {
  DRAW_TARGET = DOUBLE_BUFFER;
  DRAW_TARGET_PIXEL_HEIGHT = DOUBLE_BUFFER_PIXEL_HEIGHT;
}

void drawPoint(int x, int y) {
  if (x < 0 || x > DRAW_TARGET_PIXEL_WIDTH || y < 0 ||
      y > DRAW_TARGET_PIXEL_HEIGHT)
    return;
  int xstart = x / 8;
  int xoffset = x % 8;
  unsigned char *dst = DRAW_TARGET + y * DRAW_TARGET_BYTE_WIDTH + xstart;
  *dst |= ((unsigned char)0x01 << (7 - xoffset));
}

int drawChar(int x, int y, int c) {
  // if (c == ' ') return GWIDTH;
  char *src = nitramfont5 + c * GHEIGHT;
  for (int row = 0; row < GHEIGHT; row++, src++) {
    unsigned char charline = *src;
    for (int col = 0; col < GWIDTH; col++) {
      if ((charline & (0x01 << col)) == (0x01 << col)) {
        drawPoint(x + col, y + row);
      }
    }
  }
  return GWIDTH;
}
// Reverses the bits of a single byte (MSB <-> LSB)
unsigned char reverse_byte_bits(unsigned char b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}

unsigned char reversed[256];

int drawCharOptimised(int x, int y, int c) {
  // not visible at all
  // if ( c == ' '|| x < -GWIDTH || x > DRAW_TARGET_PIXEL_WIDTH)
  //  return GWIDTH;

    if (c == '.') { // 46 period
      drawPoint(x + 3, y + 4);
      return GWIDTH;
    } else if (c == '-') { // 45 minus
      drawPoint(x + 1, y + 2);
      drawPoint(x + 2, y + 2);
      drawPoint(x + 3, y + 2);
      return GWIDTH;
    } else if (c == ':') { // 58 colon  
      drawPoint(x + 2, y + 1);
      drawPoint(x + 2, y + 3);
      return GWIDTH;
     } else if (c == ',') { // 44 comma
      drawPoint(x + 2, y + 3);
      drawPoint(x + 3, y + 4);
      return GWIDTH;
    } else if (c == ';') { // 39 singlequote
      drawPoint(x + 1, y);
      drawPoint(x + 2, y + 2);
      return GWIDTH;
    } else if (c == '|') { // 124 pipe
      drawPoint(x + 2, y);
      drawPoint(x + 2, y + 1);
      drawPoint(x + 2, y + 2);
      drawPoint(x + 2, y + 4);
      return GWIDTH;
    }

      // partially visible, discard
  if (x < GWIDTH || x > (DRAW_TARGET_PIXEL_WIDTH - GWIDTH)) {
    drawChar(x, y, c);
     return GWIDTH;
  }


  unsigned char *src = nitramfont5 + c * GHEIGHT;
  unsigned char offset = (unsigned char)(x % 8);
  unsigned char *dst = DRAW_TARGET + y * SCREEN_WIDTH_BYTES + x / 8;
  unsigned char mask = (unsigned char)((1 << GWIDTH) - 1);

  for (int row = 0; row < GHEIGHT; row++) {
    unsigned char glyph = reversed[src[row] & mask];
    dst[0] |= (unsigned char)(glyph >> offset);
    if (offset != 0)
      dst[1] |= (unsigned char)(glyph << (8 - offset));
    dst += SCREEN_WIDTH_BYTES;
  }
  return GWIDTH;
}



int drawString(const char *str, int x, int y) {
  unsigned int sx = x;
  unsigned char c;
  while ((c = *str++) != 0) {
    if (c == '\n') {
      x = sx;
      y += THEIGHT;
    } else if (c == ' ') {
      x += GWIDTH + ADVANCEX; 
    } else {
      x += drawCharOptimised(x, y, c) + ADVANCEX;
    }
  }
  return x - sx;
}

int drawStringCenteredH(const char *str, int len, int y) {
  if (len == -1)
    len = strlen(str);
  int x = (DRAW_TARGET_PIXEL_WIDTH - ((GWIDTH + ADVANCEX) * len)) / 2;
  return drawString(str, x, y);
}

void set_color_mode() { *PRC &= ~0x01; }
void set_form_mode() { *PRC |= 0x01; }

// no checksfor speed
void fill_band(unsigned char value, int y, unsigned int height) {
  int nbytes = height*DRAW_TARGET_BYTE_WIDTH;
  unsigned char *dst = DRAW_TARGET + y * DRAW_TARGET_BYTE_WIDTH;
  memset(dst, value, nbytes);
}

// fillrectangle with nearest byte alignment, no checks for speed
void fill_rectangle_byte(unsigned char value, int x, int y, unsigned int width,
                         unsigned int height) {
  if (x < 0) {
    if (x + width <= 0) {
      return;
    }
    width = width + x;
  }
  int bytewdith = width / 8 + (width % 8 ? 1 : 0);
  for (int row = 0; row < height; row++) {
    unsigned char *dst =
        DRAW_TARGET + (y + row) * DRAW_TARGET_BYTE_WIDTH + x / 8;
        memset(dst, value, bytewdith);
  }
}

static unsigned char *fptr;
void RESET_INPUT() {
  fptr = (unsigned char*)asciimation8;
}

unsigned char GET_NEXT_VALUE() { return *fptr++; }

#define FOREGROUND 0xff
#define BACKGROUND 0x00

char *format_number(unsigned int num) {
  static char buffer[6];
  int i = 5; // Sfart at the last index

  buffer[i--] = '\0'; // Null-terminate the end

  if (num == 0) {
    buffer[i--] = '0';
  } else {
    while (num > 0) {
      buffer[i--] = (char)(num % 10) + '0';
      num /= 10;
    }
  }

  // Return the exact address where the digits actually begin
  return &buffer[i + 1];
}

unsigned int decode(unsigned char c, unsigned char *ptr) {
  if (c == 3) {
    unsigned char repeat = GET_NEXT_VALUE();
    c = GET_NEXT_VALUE();
    memset(ptr, c, repeat);
    return repeat;
  } else if (c >= 4 && c <= 8) {
    static char val[5] = {95, 92, 45, 124, 32};
    // repeat = 2;
    c = val[8 - c];
    ptr[0] = c;
    ptr[1] = c;
    return 2;
  } else if (c == 9) {
    // repeat = 3;
    // c = ' ';
    ptr[0] = ' ';
    ptr[1] = ' ';
    ptr[2] = ' ';
    return 3;
  } else if (c >= 11 && c <= 23) {
    unsigned char repeat = c - 7;
    c = ' ';
    memset(ptr, c, repeat);
    return repeat;
  }
  ptr[0] = c;
  return 1;
}

// hodls the decoded frame
unsigned char FRAME[(FRAME_CHAR_WIDTH + 1) * FRAME_CHAR_HEIGHT + 1];
// 0 end of anim
int get_next_frame() {
  unsigned int cline = 0;
  unsigned int clinelen = 0;
  unsigned char *ptr = FRAME;
  while (cline < FRAME_CHAR_HEIGHT) {
    unsigned char c = GET_NEXT_VALUE();
    if (c == 0) {
      return 0;
    }
    unsigned int decoded = decode(c, ptr);
    clinelen += decoded;
    ptr += decoded;
    if (c == '\n') {
      cline += 1;
      clinelen = 0;
    }
  }
  *ptr = 0;
  return 1;
}

void init_all() {
  *PRC = 0x00;
  *VIDEO_REG |= 0x01;
  // mute_b eep
  *((unsigned char *)0xA7C1) = 0x00;
  unsigned int i;
  for (i = 0; i < 256; i++) {
    reversed[i] = reverse_byte_bits((unsigned char)i);
  }
  set_color_mode();

  fill_band(COLOR(C_BLACK, C_GREEN), 0, BAND_PIXEL_HEIGHT);
  fill_band(COLOR(C_BLACK, C_WHITE), BAND_PIXEL_HEIGHT, FRAME_PIXEL_HEIGHT);
  fill_band(COLOR(C_BLACK, C_GREEN), BAND_PIXEL_HEIGHT + FRAME_PIXEL_HEIGHT,
            BAND_PIXEL_HEIGHT);
  set_vram_target();
  //  fixed contents
  set_form_mode();
  // clear all
  fill_band(BACKGROUND, 0, SCREEN_PIXEL_HEIGHT);
  // we are only changing form from here
  drawStringCenteredH("ASCII Wars", 10, (BAND_PIXEL_HEIGHT - GHEIGHT) / 2);
  drawStringCenteredH("Animation: Simon Jensen | Code: Frederic Delhoume", 49,
                      SCREEN_PIXEL_HEIGHT - (BAND_PIXEL_HEIGHT / 2) - GHEIGHT);
  drawStringCenteredH("  www.asciimation.co.nz | github.com/delhoume    ", 49,
                      SCREEN_PIXEL_HEIGHT - (BAND_PIXEL_HEIGHT / 2) + GHEIGHT);
  RESET_INPUT();
}

#define FPS 25
#define FRAME_DURATION 40
void WAIT_DELAY(unsigned int frames) {
  while (frames--) {
    mo5_wait_vbl();
  }
}
int main(void) {
  init_all();
  int curf = 0;
  while (1) {
    if (get_next_frame() == 0) {
      RESET_INPUT();
      curf = 0;
    } else {
     //  in double buffer
      set_double_buffer_target();
      for (int l = 0; l < FRAME_CHAR_HEIGHT; l++) {
        fill_band(BACKGROUND, l * THEIGHT, GHEIGHT);
      }
      // frame  content display
      drawString((const char *)FRAME, FRAME_START_X, 0);
      mo5_wait_vbl();
    //  cmoc optimized function
      memcpy((char *)VRAM + (FRAME_START_Y * SCREEN_WIDTH_BYTES), (const char *)DOUBLE_BUFFER, DOUBLE_BUFFER_SIZE);     
      
       if (show_info) {
        set_vram_target();
        fill_rectangle_byte(BACKGROUND, 16, 10, TWIDTH * 10, GHEIGHT);
        int x = 16;
        x += drawString("f:", x, 10);
        x += drawString(format_number(curf + 1), x, 10);
        x += drawString(" d:", x, 10);
        x += drawString(format_number(delays[curf]), x, 10);
      }
      // TODO
      //   WAIT_DELAY(delays[curf]);
      curf += 1;
    }
  }
  return 0;
}