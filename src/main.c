/*
 */

#include "asciimation8_50.h"
#include "delays_50.h"
#include "nitram5x5_mini.h"
#include <cmoc.h>
#include <mo5_video.h>
#include <mo5_audio.h>

 #define SCREEN_PIXEL_WIDTH 320
  #define SCREEN_PIXEL_HEIGHT 200
 #define GWIDTH 5
#define ADVANCEX 1
 #define GHEIGHT 5
 #define ADVANCEY 1

 // = GWIDTH + ADVANCEX;
#define TWIDTH 6
#define THEIGHT 6

#define FRAME_CHAR_WIDTH 67
#define FRAME_CHAR_HEIGHT 13
int FRAME_PIXEL_WIDTH = FRAME_CHAR_WIDTH * TWIDTH;
int FRAME_PIXEL_HEIGHT = FRAME_CHAR_HEIGHT * THEIGHT;
 int BAND_PIXEL_HEIGHT = (SCREEN_PIXEL_HEIGHT - FRAME_PIXEL_HEIGHT) / 2;
 int FRAME_START_Y = (SCREEN_PIXEL_HEIGHT - FRAME_PIXEL_HEIGHT) / 2;
 int FRAME_START_X = (SCREEN_PIXEL_WIDTH - FRAME_PIXEL_WIDTH)  / 2;

int DOUBLE_BUFFER_PIXEL_HEIGHT = FRAME_CHAR_HEIGHT * GHEIGHT;
// DOUBLE_BUFFER_PIXEL_HEIGHT * SCREEN_WIDTH_BYTES; // 3120
//3120
#define DOUBLE_BUFFER_SIZE 40

static unsigned char DOUBLE_BUFFER[DOUBLE_BUFFER_SIZE];
unsigned char* DRAW_TARGET = VRAM;
unsigned int DRAW_TARGET_PIXEL_HEIGHT = SCREEN_PIXEL_HEIGHT;
unsigned int DRAW_TARGET_BYTE_WIDTH = SCREEN_WIDTH_BYTES;
unsigned int DRAW_TARGET_PIXEL_WIDTH = SCREEN_PIXEL_WIDTH;

void set_vram_target() { 
  DRAW_TARGET = VRAM; 
  DRAW_TARGET_PIXEL_HEIGHT = SCREEN_PIXEL_HEIGHT;
}

void set_double_buffer_target() { 
 // DRAW_TARGET = DOUBLE_BUFFER;
  //RAW_TARGET_PIXEL_HEIGHT = DOUBLE_BUFFER_PIXEL_HEIGHT;
}

void drawPoint(int x, int y) {
  if (x < 0 || x > DRAW_TARGET_PIXEL_WIDTH || y < 0 || y > DRAW_TARGET_PIXEL_HEIGHT)
    return;
  int xstart = x / 8;
  int xoffset = x % 8;
  unsigned char *dst = DRAW_TARGET + y * DRAW_TARGET_BYTE_WIDTH + xstart;
  *dst |= ((unsigned char)0x01 << (7 - xoffset));
}

int drawChar(int x, int y, int c) {
  //if (c == ' ') return GWIDTH;

 unsigned char *src = nitramfont5 + c *  GHEIGHT;
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

unsigned char reversed[128];

int drawCharOptimised(int x, int y, int c) {
  // not visle at all
  if ( /* c == ' '|| */ x < -GWIDTH || x > DRAW_TARGET_PIXEL_WIDTH)
    return GWIDTH;
  
    // partially visible
  if (x < GWIDTH  || x >  (DRAW_TARGET_PIXEL_WIDTH - GWIDTH))
    return drawChar(x, y, c);

   unsigned char *src = nitramfont5 + c * GHEIGHT;
  unsigned char offset = (unsigned char)(x % 8);
  unsigned char *dst = VRAM + y * SCREEN_WIDTH_BYTES + x / 8;
  unsigned char mask =  ( unsigned char)((1 << GWIDTH) - 1);

  for (int row = 0; row < GHEIGHT; row++) {
     unsigned char glyph = reversed[src[row] & mask];
    dst[0] |= (unsigned char)(glyph >>  offset);
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
  x  += drawCharOptimised(x, y, c) + ADVANCEX;
  }
  return x-sx;
}

int drawstringCenteredH(const char *str, int len, int y) {
  if (len == -1)
    len = strlen(str);
  int x = (DRAW_TARGET_PIXEL_WIDTH - ((GWIDTH + ADVANCEX) * len)) / 2;
  return drawString(str, x, y);
}

void set_color_mode() { *PRC &= ~0x01; }
void set_form_mode() { *PRC |=  0x01; } 

// no checksfor speed
void fill_band(unsigned char value, int y, unsigned int height) {
   int nbytes = (height) * DRAW_TARGET_BYTE_WIDTH; 
  unsigned char* dst = DRAW_TARGET + y * DRAW_TARGET_BYTE_WIDTH;
  while(nbytes--) *dst++ = value;
}


static unsigned char *fptr;
void RESET_INPUT() { fptr = asciimation8_50; }

unsigned char GET_NEXT_VALUE() { return *fptr++; }

#define FOREGROUND 0xff
#define BACKGROUND 0x00

char* format_number(unsigned int num) {
    static char buffer[6];
    int i = 5; // Start at the last index
    
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


void init_all() {
  *PRC = 0x00;
  *VIDEO_REG |= 0x01;
// mute_beep
  *((unsigned char*)0xA7C1) = 0x00;
  for (unsigned int i = 0 ; i < 256; i++) {
    reversed[i] = reverse_byte_bits((unsigned char)i);
  }
  mo5_wait_vbl();
  set_color_mode();
  set_vram_target();
  fill_band(COLOR(C_BLACK, C_GREEN), 0, BAND_PIXEL_HEIGHT);
  fill_band(COLOR(C_BLACK, C_WHITE), BAND_PIXEL_HEIGHT, FRAME_PIXEL_HEIGHT);
  fill_band(COLOR(C_BLACK, C_GREEN),  BAND_PIXEL_HEIGHT + FRAME_PIXEL_HEIGHT, BAND_PIXEL_HEIGHT);
  // we are only changing form from here
  //  fixed contents
  set_form_mode();
  // clear all
  fill_band(BACKGROUND, 0, SCREEN_PIXEL_HEIGHT);
  // we are only changing form from here
  drawstringCenteredH("ASCII Wars", -1, (BAND_PIXEL_HEIGHT - GHEIGHT) / 2);
  drawstringCenteredH("Animation: Simon Jensen | Code: Frederic Delhoume", -1,
                      SCREEN_PIXEL_HEIGHT - (BAND_PIXEL_HEIGHT / 2) - GHEIGHT);
  drawstringCenteredH("  www.asciimation.co.nz | github.com/delhoume    ", -1,
                      SCREEN_PIXEL_HEIGHT - (BAND_PIXEL_HEIGHT / 2) + GHEIGHT);
  RESET_INPUT();
}

#define FPS 25
#define FRAME_DURATION 40 
void WAIT_DELAY(unsigned int frames) {
  while (frames--)
  {
    mo5_wait_vbl();
  }
}

int main(void) {
  init_all();
  int curx = FRAME_START_X;
  int cury = FRAME_START_Y;
  int curl = 0;
  int curf = 0;
  unsigned char c = GET_NEXT_VALUE();
  while (1) {
    if (c == '\n') {
      curx = FRAME_START_X;
      cury += GHEIGHT + ADVANCEY;
      if (curl == FRAME_CHAR_HEIGHT - 1) {
        cury = FRAME_START_Y;
        curl = 0;       
         fill_band(BACKGROUND, THEIGHT, THEIGHT * 2);
        drawstringCenteredH("frame", 5, THEIGHT);
        drawstringCenteredH(format_number(curf), -1, THEIGHT * 2);
        WAIT_DELAY(delays[curf]);
        //fill_band(BACKGROUND, cury, GHEIGHT);
     mo5_wait_vbl();
        curf += 1;
        } else {
        curl += 1;
      }
      fill_band(BACKGROUND, cury, GHEIGHT);
    } else {
      int rep = 1;
      // all rle opcodes
      if (c == 3) {
        // repeat
        rep = GET_NEXT_VALUE();
        c = GET_NEXT_VALUE();
      } else if (c >= 4 && c <= 8) {
        static char val[5] = { 95, 92, 45, 124, 32 };
        rep = 2;
        c = val[8 - c];
      } else if (c == 9) {
        rep = 3;
        c = ' ';
      } else if (c >= 11 && c <= 23) {
        rep = c - 7;
        c = ' ';
      }
      for (int r = 0; r < rep; ++r) {
        curx += drawCharOptimised(curx, cury, c) + ADVANCEX;
      }
    }
    c = GET_NEXT_VALUE();
    if (c == 0) {
      curx = FRAME_START_X;
      cury = FRAME_START_Y;
      curf = 0;
      curl = 0;
      RESET_INPUT();
      c = GET_NEXT_VALUE();
    }
  }
  return 0;
}