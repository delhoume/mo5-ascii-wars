/*
 * Frederic Delhoume 2026
 *
 */

 // cmoc optimized functions

#if defined(__CMOC__)
void *memcpy(void *destination, const void *source, unsigned int count);
void *memset(void *destination, int value, unsigned int count);
#else
#include <string.h>
#endif

typedef unsigned char uint8_t;
///
#include "asciimation7_75.h"
// #include "asciimation8_50.h"
#include "delays_75.h"
//  #include "short8.h"
// #include "short_delays.h"


#include "cmoc.h"
#include "nitram5x5_mini.h"

// ============================================================================
// HARDWARE REGISTERS
// ============================================================================

#define PRC       ((unsigned char *)0xA7C0)  // VRAM bank select (bit0: 0=color, 1=form)
#define VIDEO_REG ((unsigned char *)0xA7E7)  // Video mode register
#define VRAM      ((unsigned char *)0x0000)  // Video memory base address
#define VBL_REG   ((unsigned char *)0xA7E7)  // VBL status register (bit7=1 during blanking)
#define VBL_BIT   0x80

void my_wait_vbl(void)
{
    while ( *VBL_REG &  VBL_BIT) ;
    while (!(*VBL_REG & VBL_BIT)) ;
}
// ============================================================================
// 16-COLOR PALETTE
// ============================================================================

#define C_BLACK        0
#define C_RED          1
#define C_GREEN        2
#define C_YELLOW       3
#define C_BLUE         4
#define C_MAGENTA      5
#define C_CYAN         6
#define C_WHITE        7
#define C_GRAY         8
#define C_LIGHT_RED    9
#define C_LIGHT_GREEN  10
#define C_LIGHT_YELLOW 11
#define C_LIGHT_BLUE   12
#define C_PURPLE       13
#define C_LIGHT_CYAN   14
#define C_ORANGE       15

/** Builds a MO5 color byte in FFFFBBBB format (fg in bits 4-7, bg in bits 0-3). */
#define COLOR(bg, fg) (unsigned char)(((bg) & 0x0F) | (((fg) & 0x0F) << 4))

// ============================================================================
// SCREEN DIMENSIONS
// ============================================================================

#define SCREEN_WIDTH_BYTES  40    // 320 pixels / 8 pixels per byte
#define SCREEN_HEIGHT       200   // Pixel rows
#define SCREEN_SIZE_BYTES   (SCREEN_WIDTH_BYTES * SCREEN_HEIGHT)  // 8000 bytes
#define SCREEN_PIXEL_WIDTH 320
#define SCREEN_PIXEL_HEIGHT 200
#define GWIDTH 5
#define ADVANCEX 0
#define GHEIGHT 5
#define ADVANCEY 1

// = GWIDTH + ADVANCEX;
#define TWIDTH 5
#define THEIGHT 6

#define FRAME_CHAR_WIDTH 67
#define FRAME_CHAR_HEIGHT 13
#define FRAME_PIXEL_WIDTH (FRAME_CHAR_WIDTH * TWIDTH)
#define FRAME_PIXEL_HEIGHT (FRAME_CHAR_HEIGHT * THEIGHT)
#define BAND_PIXEL_HEIGHT ((SCREEN_PIXEL_HEIGHT - FRAME_PIXEL_HEIGHT) / 2)
#define FRAME_START_Y ((SCREEN_PIXEL_HEIGHT - FRAME_PIXEL_HEIGHT) / 2)
#define FRAME_START_X ((SCREEN_PIXEL_WIDTH - FRAME_PIXEL_WIDTH) / 2)

#include "ascii_decoder.h"

unsigned char *DRAW_TARGET = VRAM;
unsigned int DRAW_TARGET_PIXEL_HEIGHT = SCREEN_PIXEL_HEIGHT;
unsigned int DRAW_TARGET_BYTE_WIDTH = SCREEN_WIDTH_BYTES;
unsigned int DRAW_TARGET_PIXEL_WIDTH = SCREEN_PIXEL_WIDTH;
unsigned int DRAW_TARGET_Y_OFFSET = 0; 
unsigned int show_frames = 1;


#define FOREGROUND 0xff
#define BACKGROUND 0x00


void set_vram_target() {
  DRAW_TARGET = VRAM;
  DRAW_TARGET_PIXEL_HEIGHT = SCREEN_PIXEL_HEIGHT;
  DRAW_TARGET_Y_OFFSET = 0;
  DRAW_TARGET_BYTE_WIDTH = SCREEN_WIDTH_BYTES;
}

// terminal drawing frunctions must use DRAW_TARGET_Y_OFFSET;
void drawPoint(int x, int y) {
  y -= DRAW_TARGET_Y_OFFSET;

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
      if ((charline & (0x80 >> col)) != 0) {
        drawPoint(x + col, y + row);
      }
    }
  }
  return GWIDTH;
}
//  Reverses the bits of a single byte (MSB <-> LSB)
unsigned char reverse_byte_bits(unsigned char b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}

#define FIRST_FONT_CHAR 32
#define FONT_GLYPH_COUNT 96

// no checksfor speed
void fillBand(unsigned char value, int y, unsigned int height) {
  y -= DRAW_TARGET_Y_OFFSET;
  int nbytes = height * DRAW_TARGET_BYTE_WIDTH;
  unsigned char* dst = DRAW_TARGET + y * DRAW_TARGET_BYTE_WIDTH;
   memset(dst, value, nbytes);
}

void clearTarget() {
  memset(DRAW_TARGET, BACKGROUND, DRAW_TARGET_BYTE_WIDTH * DRAW_TARGET_PIXEL_HEIGHT);
}

int drawCharOptimised(int x, int y, int c) {
  // not visible at all
 if ( c == ' ') return GWIDTH;
 if (c < FIRST_FONT_CHAR || c >= FIRST_FONT_CHAR + FONT_GLYPH_COUNT)
   return GWIDTH;
// if (c == '\n') return 0;
 if  (x < 0) {
  if (x >= -GWIDTH) return GWIDTH; ///drawChar(x, y, c);// partially visible
  if (x <-GWIDTH) return GWIDTH; // not visible
 } else {
  if (x > DRAW_TARGET_PIXEL_WIDTH) return GWIDTH; // not visible
 if  (x > (DRAW_TARGET_PIXEL_WIDTH - GWIDTH)) return GWIDTH; // return drawChar(x, y, c);
  }
// char is in visible diplay

if (c == '.') {
  int dot_x = x + 3;
  int dot_y = y + 4 - DRAW_TARGET_Y_OFFSET;
  if (dot_x >= 0 && dot_x < DRAW_TARGET_PIXEL_WIDTH && dot_y >= 0 &&
      dot_y < DRAW_TARGET_PIXEL_HEIGHT) {
    unsigned char *dst =
        DRAW_TARGET + dot_y * DRAW_TARGET_BYTE_WIDTH + dot_x / 8;
    *dst |= (unsigned char)(0x80 >> (dot_x % 8));
  }
  return GWIDTH;
  }
    y-= DRAW_TARGET_Y_OFFSET;
  unsigned char offset = (unsigned char)(x % 8);
  unsigned char *dst = DRAW_TARGET + y * DRAW_TARGET_BYTE_WIDTH + x / 8;
  unsigned char *glyph = nitramfont5 + c * GHEIGHT;
  if (offset == 0) {
    dst[0] = (dst[0] & 0x07) | glyph[0];
    dst += DRAW_TARGET_BYTE_WIDTH;
    dst[0] = (dst[0] & 0x07) | glyph[1];
    dst += DRAW_TARGET_BYTE_WIDTH;
    dst[0] = (dst[0] & 0x07) | glyph[2];
    dst += DRAW_TARGET_BYTE_WIDTH;
    dst[0] = (dst[0] & 0x07) | glyph[3];
    dst += DRAW_TARGET_BYTE_WIDTH;
    dst[0] = (dst[0] & 0x07) | glyph[4];
  } else {
    dst[0] |= (unsigned char)(glyph[0] >> offset);
    dst[1] |= (unsigned char)(glyph[0] << (8 - offset));
    dst += DRAW_TARGET_BYTE_WIDTH;
    dst[0] |= (unsigned char)(glyph[1] >> offset);
    dst[1] |= (unsigned char)(glyph[1] << (8 - offset));
    dst += DRAW_TARGET_BYTE_WIDTH;
    dst[0] |= (unsigned char)(glyph[2] >> offset);
    dst[1] |= (unsigned char)(glyph[2] << (8 - offset));
    dst += DRAW_TARGET_BYTE_WIDTH;
    dst[0] |= (unsigned char)(glyph[3] >> offset);
    dst[1] |= (unsigned char)(glyph[3] << (8 - offset));
    dst += DRAW_TARGET_BYTE_WIDTH;
    dst[0] |= (unsigned char)(glyph[4] >> offset);
    dst[1] |= (unsigned char)(glyph[4] << (8 - offset));
  }
  return GWIDTH;
}

int drawString(const unsigned char *str, int x, int y) {
  int sx = x;
  unsigned char c;
  while ((c = *str++) != 0) {
    if (c == '\n') {
      x = sx;
      y += THEIGHT;
    } else {
      x += drawCharOptimised(x, y, c) + ADVANCEX;
    }
  }
  return x - sx;
}

#define DOUBLE_BUFFER_SIZE (THEIGHT * FRAME_CHAR_HEIGHT * SCREEN_WIDTH_BYTES)
unsigned char FRAME_BUFFER[DOUBLE_BUFFER_SIZE];

void setFrameTarget() {
  DRAW_TARGET = FRAME_BUFFER;
  DRAW_TARGET_PIXEL_HEIGHT = FRAME_PIXEL_HEIGHT;
  DRAW_TARGET_BYTE_WIDTH = SCREEN_WIDTH_BYTES;
  DRAW_TARGET_Y_OFFSET = FRAME_START_Y;
}

void commitFrameTarget() {
    unsigned char* dst = VRAM + (FRAME_START_Y * SCREEN_WIDTH_BYTES);
    unsigned char* src= FRAME_BUFFER;
    unsigned int size =  FRAME_PIXEL_HEIGHT * SCREEN_WIDTH_BYTES;
    memcpy(dst, src, size);
}

// not valid when we have a string with newliness
int drawStringCenteredH(const unsigned char *str, int len, int y) {
  if (len == -1)
    len = strlen((const char*)str);
  int x = (DRAW_TARGET_PIXEL_WIDTH - ((TWIDTH) * len)) / 2;
  return drawString(str, x, y);
}


void set_color_mode() { *PRC &= ~0x01; }
void set_form_mode() { *PRC |= 0x01; }

unsigned char buffer[6]; // 5 digits + null terminator

const unsigned char *format_number(unsigned int num) {
  unsigned int i = 5; // Start filling from the end of the buffer
  buffer[i--] = 0 ; // Null-terminate the string
  if (num==0) {
    buffer[i--] = '0';
  } else {  
    while (num > 0) {
      buffer[i--] = ((unsigned char)(num % 10)) + '0';
      num /= 10;
    }
  }
  // Return the exact address where the digits actually begin
  return buffer + i + 1;
}

#define LINE_CHARS_SIZE 6
#define LINE_BUFFER_SIZE (GHEIGHT * LINE_CHARS_SIZE)
unsigned char LINE_BUFFER[LINE_BUFFER_SIZE];

void setText10Target() {
  DRAW_TARGET = LINE_BUFFER;
  DRAW_TARGET_PIXEL_HEIGHT = GHEIGHT;
  DRAW_TARGET_BYTE_WIDTH = LINE_CHARS_SIZE;
  DRAW_TARGET_Y_OFFSET = 10;
};

// Enough for 10 chars
void commitText10Target() {
  for (int row = 0; row < GHEIGHT; row++) {
    memcpy(VRAM + (row + 10) * SCREEN_WIDTH_BYTES + 1,
           LINE_BUFFER + row * LINE_CHARS_SIZE + 1, 4);
  }
}

void init_all() {
  *PRC = 0x00;
  *VIDEO_REG |= 0x01;
  // mute_beep
  *((unsigned char *)0xA7C1) = 0x00;
  unsigned int i;
  unsigned char mask = (unsigned char)((1 << GWIDTH) - 1);
  for (i = 0; i < sizeof(nitramfont5); i++) {
    nitramfont5[i] = reverse_byte_bits(nitramfont5[i] & mask);
  }

  set_vram_target();
  set_color_mode();

  fillBand(COLOR(C_BLACK, C_GREEN), 0, BAND_PIXEL_HEIGHT);
  fillBand(COLOR(C_BLACK, C_WHITE), BAND_PIXEL_HEIGHT, FRAME_PIXEL_HEIGHT);
   fillBand(COLOR(C_BLACK, C_GREEN), BAND_PIXEL_HEIGHT + FRAME_PIXEL_HEIGHT,
             BAND_PIXEL_HEIGHT);

  //  fixed contents
  set_form_mode();
  // clear all
  clearTarget(BACKGROUND);
  // we are only changing form from here

    drawStringCenteredH((const unsigned char*)"MO5 ASCII Wars", 10, (BAND_PIXEL_HEIGHT - GHEIGHT) / 2);
  drawStringCenteredH((const unsigned char*)"Animation: Simon Jensen | Code: Frederic Delhoume", 49,
                      SCREEN_PIXEL_HEIGHT - (BAND_PIXEL_HEIGHT / 2) - GHEIGHT);
  drawStringCenteredH((const unsigned char*)"  www.asciimation.co.nz | github.com/delhoume    ", 49,
                      SCREEN_PIXEL_HEIGHT - (BAND_PIXEL_HEIGHT / 2) + GHEIGHT);
  RESET_INPUT(asciimation7, sizeof(asciimation7), 7);
}

#define FPS 25
#define FRAME_DURATION 40
void WAIT_DELAY(unsigned int frames) {
  while (frames--) {
    my_wait_vbl();
  }
}

int main(void) {
  init_all();
  int curf = 0;

  while (1) {
    if (get_next_frame() == 0) {
      if (FRAME_DECODE_ERROR)
        while (1) {
        }
      RESET_INPUT(asciimation7, sizeof(asciimation7), 7);
      curf = 0;
    } else {
      if (show_frames) {
        setText10Target();
        clearTarget();
        drawString((const unsigned char*)format_number(curf), 8, 10);
      }

      setFrameTarget();
      int ypos = FRAME_START_Y;
      for (unsigned char line = 0; line < FRAME_CHAR_HEIGHT; line++) {
        fillBand(BACKGROUND, ypos, GHEIGHT);
        drawString(FRAME[line], FRAME_START_X, ypos);
        ypos += THEIGHT;
      }
      my_wait_vbl();
      commitFrameTarget();
      if (show_frames)
        commitText10Target();
      WAIT_DELAY(delays[curf]);
      curf += 1;
    }
  }

  return 0;
}