#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FRAME_CHAR_WIDTH 67
#define FRAME_CHAR_HEIGHT 13

#include "short8.h"
#include "ascii_decoder.h"

int main(void) {
  RESET_INPUT(asciimation8, sizeof(asciimation8), 8);

  while (get_next_frame()) {
    for (unsigned char row = 0; row < FRAME_CHAR_HEIGHT; row++) {
      if (fwrite(FRAME[row], 1, LINELEN[row], stdout) != LINELEN[row] ||
          putchar('\n') == EOF)
        return 1;
    }
  }

  if (FRAME_DECODE_ERROR) {
    fputs("Invalid or oversized encoded frame data\n", stderr);
    return 1;
  }

  return ferror(stdout) ? 1 : 0;
}
