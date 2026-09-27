#include <SDL2/SDL.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 25fps
#define TICK_MS 40

#include "nitram5x5.h"
#include "sans512_8.h"

#include "delays.h"

#define BACKGROUND_COLOR C_BLACK
#define FOREGROUND_COLOR C_WHITE
#define ALLBACKGROUND 0x00
#define ALLFOREGROUND 0xff
#define DARKPATTERN 0x0a
#define LIGHTPATTERN 0x02

#define COLOR(fore, back) ((back) << 4 | (fore))

// multiple of 8, need 330 with advancex = 0
// or text to be readable, advancex mus be 1, this makes final width 4058
#if defined(MO5)
static const int WIDTH = 320;
#else
static const int WIDTH = 408;
#endif

const int HEIGHT = 200;
static const int ADVANCEX = 1;
static const int ADVANCEY = 1;

static const int BYTES_WIDTH = WIDTH / 8;
static const int SCREEN_SIZE_BYTES = BYTES_WIDTH * HEIGHT;

static unsigned char VRAM_FORM[SCREEN_SIZE_BYTES];
static unsigned char VRAM_COLOR[SCREEN_SIZE_BYTES];

#define FRAME_CHAR_WIDTH 67
#define FRAME_CHAR_HEIGHT 12
#define TOTAL_FRAMES 3410

static int save_frames = 0;
static int cframe = 0;
static SDL_Window *g_window = NULL;

unsigned char *VRAM = VRAM_FORM;
void set_form_mode(void) { VRAM = VRAM_FORM; }
void set_color_mode(void) { VRAM = VRAM_COLOR; }

typedef struct {
  unsigned char r;
  unsigned char g;
  unsigned char b;
} RGB;

RGB mo5palette[16] = {
    {0, 0, 0},       // 0: black
    {255, 0, 0},     // 1: red
    {0, 255, 0},     // 2: green
    {255, 255, 0},   // 3: yellow
    {0, 0, 255},     // 4: blue
    {255, 0, 255},   // 5: magenta
    {0, 255, 255},   // 6: cyan
    {255, 255, 255}, // 7: white
    {128, 128, 128}, // 8: gray
    {255, 128, 128}, // 9: light red
    {128, 255, 128}, // 10: light green
    {255, 255, 128}, // 11: light yellow
    {128, 128, 255}, // 12: light blue
    {255, 128, 255}, // 13: purple
    {128, 255, 255}, // 14: light cyan
    {255, 128, 0}    // 15: orange
};

#define C_BLACK 0
#define C_RED 1
#define C_GREEN 2
#define C_YELLOW 3
#define C_BLUE 4
#define C_MAGENTA 5
#define C_CYAN 6
#define C_WHITE 7
#define C_GRAY 8
#define C_LIGHT_RED 9
#define C_LIGHT_GREEN 10
#define C_LIGHT_YELLOW 11
#define C_LIGHT_BLUE 12
#define C_PURPLE 13
#define C_LIGHT_CYAN 14
#define C_ORANGE 15

static void present_vram(SDL_Renderer *renderer, SDL_Texture *texture) {
  uint32_t pixels[WIDTH * HEIGHT];
  uint32_t palette[16];

  for (int color = 0; color < 16; color++) {
    palette[color] = 0xff000000u | ((uint32_t)mo5palette[color].r << 16) |
                     ((uint32_t)mo5palette[color].g << 8) | mo5palette[color].b;
  }

  for (int y = 0; y < HEIGHT; y++) {
    int row_offset = y * BYTES_WIDTH;
    int pixel_offset = y * WIDTH;
    for (int byte = 0; byte < BYTES_WIDTH; byte++) {
      int index = row_offset + byte;
      unsigned char form_byte = VRAM_FORM[index];
      unsigned char color = VRAM_COLOR[index];
      int foreground = (color >> 4) & 0x0f;
      int background = color & 0x0f;

      for (int bit = 0; bit < 8; bit++) {
        int form = (form_byte >> (7 - bit)) & 1;
        int palette_index = form ? foreground : background;
        pixels[pixel_offset + byte * 8 + bit] = palette[palette_index];
      }
    }
  }

  SDL_UpdateTexture(texture, NULL, pixels, WIDTH * sizeof(uint32_t));
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
}

static void save_frame_png(int frame_number) {
  unsigned char pixels[WIDTH * HEIGHT * 3];

  for (int y = 0; y < HEIGHT; y++) {
    int row_offset = y * BYTES_WIDTH;
    int pixel_offset = y * WIDTH * 3;
    for (int byte = 0; byte < BYTES_WIDTH; byte++) {
      unsigned char form_byte = VRAM_FORM[row_offset + byte];
      unsigned char color = VRAM_COLOR[row_offset + byte];
      int foreground = (color >> 4) & 0x0f;
      int background = color & 0x0f;

      for (int bit = 0; bit < 8; bit++) {
        int form = (form_byte >> (7 - bit)) & 1;
        RGB rgb = mo5palette[form ? foreground : background];
        int pixel = pixel_offset + (byte * 8 + bit) * 3;
        pixels[pixel + 0] = rgb.r;
        pixels[pixel + 1] = rgb.g;
        pixels[pixel + 2] = rgb.b;
      }
    }
  }

  char filename[64];
  snprintf(filename, sizeof(filename), "frames/frame%04d.png", frame_number);
  stbi_write_png(filename, WIDTH, HEIGHT, 3, pixels, WIDTH * 3);
}

static int is_quit_event(const SDL_Event *event) {
  return event->type == SDL_QUIT ||
         (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_ESCAPE);
}

static int is_function_event(const SDL_Event *event) {
  return event->type == SDL_KEYDOWN && event->key.repeat == 0 &&
         event->key.keysym.sym == SDLK_s;
}

static void update_window_title(SDL_Window *window) {
  char title[128];
  if (window == NULL)
    return;

  if (save_frames) {
    snprintf(title, sizeof(title),
             "MO5 Animation - frame %d / %d - press S to stop saving ",
             cframe + 1, TOTAL_FRAMES);
  } else {
    snprintf(title, sizeof(title),
             "MO5 Animation - frame %d / %d - press S to start saving to PNG",
             cframe + 1, TOTAL_FRAMES);
  }
  SDL_SetWindowTitle(window, title);
}

static int process_events(void) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (is_quit_event(&event)) {
      return 0;
    }
    if (is_function_event(&event)) {
      save_frames = !save_frames;
      update_window_title(g_window);
    }
  }
  return 1;
}

static int wait_ticks(int ticks) {
  Uint32 deadline = SDL_GetTicks() + (Uint32)ticks * TICK_MS;

  while (1) {
    if (!process_events())
      return 0;

    Uint32 remaining = deadline - SDL_GetTicks();
    if ((Sint32)remaining <= 0)
      return 1;

    SDL_Event event;
    if (SDL_WaitEventTimeout(&event, (int)remaining)) {
      if (is_quit_event(&event))
        return 0;
      if (is_function_event(&event)) {
        save_frames = !save_frames;
        update_window_title(g_window);
      }
    }
  }
}

void fill_screen(unsigned char value) {
  for (int p = 0; p < SCREEN_SIZE_BYTES; p++) {
    VRAM[p] = value;
  }
}

void fill_screen_band(unsigned char value, int y, int height) {
  unsigned char *dst = VRAM + y * BYTES_WIDTH;
  int nbytes = height * BYTES_WIDTH;
  for (int p = 0; p < nbytes; p++) {
    dst[p] = value;
  }
}

typedef struct {
  char *name;
  int glyphwidth;
  int glyphheight;
  int numglyphs;
  int wrongendian;
  int advance;
  unsigned char *data;
} Font;

Font Sans8x8 = {"Sans_8x8", 8, 8, 512, 0, 1, sans512_font};
Font Nitram5x5 = {"Nitram_5x5", 5, 5, 256, 1, 1, nitramfont5};

// Reverses the bits of a single byte (MSB <-> LSB)
unsigned char reverse_byte_bits(unsigned char b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}

// Converts an entire font array in-place
void convert_font_inplace(unsigned char *font_array, size_t size) {
  for (size_t i = 0; i < size; i++) {
    font_array[i] = reverse_byte_bits(font_array[i]);
  }
}

void initFont(Font *font) {
  if (font->wrongendian == 0) {
    for (int c = 0; c < font->numglyphs * font->glyphheight; c++) {
      font->data[c] = reverse_byte_bits(font->data[c]);
    }
    font->wrongendian = 1;
  }
}

void drawPoint(int x, int y) {
  if (x < 0 || x >= WIDTH || y < 0 || y > HEIGHT)
    return;
  int xstart = x / 8;
  int xoffset = x % 8;
  unsigned char *dst = VRAM + y * BYTES_WIDTH + xstart;
  *dst |= (0x01 << (7 - xoffset));
}

void drawHLine(int x, int y, int w) {
  for (int p = 0; p < w; p++) {
    drawPoint(x + p, y);
  }
}

void drawVLine(int x, int y, int h) {
  for (int p = 0; p < h; p++) {
    drawPoint(x, y + p);
  }
}

void drawBox(int x, int y, int w, int h) {
  drawHLine(x, y, w);
  drawHLine(x, y + h, w);
  drawVLine(x, y, h);
  drawVLine(x + w, y, h);
}

// draws wxh w<8 glyphs  at any pixel position
// font width <= 8
// handles existing background
// x and are in pixel
// returns width
int drawChar(int x, int y, int c, Font *font) {
  if (c == ' ')
    return font->glyphwidth;
  if (c >= font->numglyphs)
    return 0;

  const unsigned char *src =
      font->data + c * font->glyphheight * 1; // glyph byte width
  for (int row = 0; row < font->glyphheight; row++, src++) {
    unsigned char charline = *src;
    for (int col = 0; col < font->glyphwidth; col++) {
      if ((charline & (0x01 << col)) == (0x01 << col)) {
        drawPoint(x + col, y + row);
      }
    }
  }
  return font->glyphwidth;
}

int drawCharOptimised(int x, int y, int c, Font *font) {
  if (c == ' ')
    return font->glyphwidth;
  if (c < 0 || c >= font->numglyphs)
    return 0;

  int width = font->glyphwidth;
  if (x < 0 || x + width > WIDTH || y < 0 || y + font->glyphheight > HEIGHT)
    return drawChar(x, y, c, font);

  const unsigned char *src = font->data + c * font->glyphheight;
  unsigned char offset = (unsigned char)(x % 8);
  unsigned char *dst = VRAM + y * BYTES_WIDTH + x / 8;
  unsigned char mask = width == 8 ? 0xff : (unsigned char)((1u << width) - 1u);

  for (int row = 0; row < font->glyphheight; row++) {
    unsigned char glyph = reverse_byte_bits(src[row] & mask);
    dst[0] |= (unsigned char)(glyph >> offset);
    if (offset != 0)
      dst[1] |= (unsigned char)(glyph << (8 - offset));
    dst += BYTES_WIDTH;
  }
  return width;
}

int drawString(const char *str, int x, int y, Font *font) {
  int sx = x;
  unsigned char c;
  while ((c = *str++) != 0) {
    x += drawCharOptimised(x, y, c, font) + font->advance;
  }
  return x - sx;
}

int drawstringCenteredH(const char *str, int len, int y, Font *font) {
  if (len == -1)
    len = strlen(str);
  int x = (WIDTH - ((font->glyphwidth + font->advance) * len)) / 2;
  return drawString(str, x, y, font);
}

void initFonts() {
  initFont(&Sans8x8);
  initFont(&Nitram5x5);
}

// global state variables
int g_bit_offset = 0;
unsigned int g_acc = 0;
unsigned int g_acc_bits = 0;

// #define PACKED7BIT

FILE *input = NULL;
#if defined(PACKED7BIT)
#define READ_ONE_VALUE read_7_bits_from_file()
#define INPUT "asciimation7.bin"
#else
#define READ_ONE_VALUE read_one_byte_from_file()
#define INPUT "asciimation8_50.bin"
#endif

void init_anim(void) {
  if (input == NULL) {
    input = fopen(INPUT, "rb");
    if (input == NULL) {
      fprintf(stderr, "error in init anim %s\n", INPUT);
      exit(1);
    }
  }

  rewind(input);
  g_bit_offset = 0;
  g_acc = 0;
  g_acc_bits = 0;
}

unsigned char read_one_byte_from_file() {
  unsigned char b;
  if (fread(&b, 1, 1, input) != 1)
    return 0;
  return b;
}

unsigned char read_7_bits_from_file() {
  while (g_acc_bits < 7) {
    unsigned char byte;
    if (fread(&byte, 1, 1, input) != 1)
      return 0;
    g_acc |= ((unsigned int)byte) << g_acc_bits;
    g_acc_bits += 8;
  }

  unsigned char result = (unsigned char)(g_acc & 0x7F);
  g_acc >>= 7;
  g_acc_bits -= 7;
  return result;
}

int main(int argc, char *argv[]) {
  initFonts();

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_Window *window = SDL_CreateWindow("MO5 Animation", SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED, WIDTH * 2,
                                        HEIGHT * 2, SDL_WINDOW_RESIZABLE);
  g_window = window;
  update_window_title(window);
  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  SDL_Texture *texture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                        SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

  if (!window || !renderer || !texture) {
    fprintf(stderr, "SDL setup failed: %s\n", SDL_GetError());
    if (texture)
      SDL_DestroyTexture(texture);
    if (renderer)
      SDL_DestroyRenderer(renderer);
    if (window)
      SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  // Font *font = &Sans8x8;
  Font *font = &Nitram5x5;
  unsigned char greentext = COLOR(C_BLACK, C_GREEN);
  unsigned char whitetext = COLOR(C_BLACK, C_WHITE);
  // try to center
  int frameheight = (1 + FRAME_CHAR_HEIGHT) * (font->glyphheight + ADVANCEY);
  int starty = (HEIGHT - frameheight) / 2;
  int bandheight = starty;

  int framewidth = FRAME_CHAR_WIDTH * (font->glyphwidth + ADVANCEX);
  int startx = (WIDTH - framewidth) / 2;
  int cline = 0;
  int x = startx;
  int y = starty;
  set_color_mode();
  fill_screen_band(greentext, 0, bandheight);
  fill_screen_band(whitetext, starty, frameheight);
  fill_screen_band(greentext, starty + frameheight, bandheight);
  // we are only changing form from here
  set_form_mode();
  // we are only changing form from here
  drawstringCenteredH("ASCII Wars", -1, (bandheight - font->glyphheight) / 2,
                      font);
  drawstringCenteredH("Animation: Simon Jensen | Code: Frederic Delhoume", -1,
                      HEIGHT - (bandheight / 2) - font->glyphheight, font);
  drawstringCenteredH("  www.asciimation.co.nz | github.com/delhoume    ", -1,
                      HEIGHT - (bandheight / 2) + font->glyphheight, font);
  int running = 1;
  init_anim();
  unsigned char c = READ_ONE_VALUE;
  while (running) {
    running = process_events();
    if (!running)
      break;
    if (c == '\n') {
      x = startx;
      y += font->glyphheight + ADVANCEY;
      if (cline == FRAME_CHAR_HEIGHT) {
        x = startx;
        y = starty;

        if (save_frames)
          save_frame_png(cframe);

        present_vram(renderer, texture);
        fill_screen_band(BACKGROUND_COLOR, starty, frameheight);
        running = wait_ticks(delays[cframe]);
        cframe += 1;
        update_window_title(window);
        cline = 0;
      } else {
        cline += 1;
      }
    } else {
      int rep = 1;
      if (c == 3) {
        // repeat
        rep = READ_ONE_VALUE;
        c = READ_ONE_VALUE;
      } else if (c >= 4 && c <= 8) {
        static char val[5] = "_\\/| ";
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
        x += drawChar(x, y, c, font) + ADVANCEX;
      }
    }
    c = READ_ONE_VALUE;
    if (c == 0) { // restart from 0
      x = startx;
      y = starty;
      cframe = 0;
      update_window_title(g_window);
      cline = 0;
      init_anim();
      c = READ_ONE_VALUE;
    }
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}