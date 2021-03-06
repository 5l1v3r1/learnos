#include <shared/addresses.h>
#include <anlock.h>
#include "libkern_base.h"
#include "stdio.h"

#define buffer ((unsigned char *)SCREEN_BUFFER)

static uint64_t printLock = 0;
static uint8_t color = 0xa;

static void setPosition(unsigned short x, unsigned short y);
static void scrollUp();

void print(const char * str) {
  anlock_lock(&printLock);
  unsigned short x = CURSOR_INFO[0], y = CURSOR_INFO[1];
  // print each character like it might be your last!
  while (str[0]) {
    unsigned char theChar = str[0];
    str++;
    if (theChar == '\n') {
      y++;
      x = 0;
    } else if (theChar == '\b') {
      if (x == 0) {
        if (y != 0) {
          y--;
          // find the last character on this line
          x = SCREEN_WIDTH - 1;
          int i, newLoc = (SCREEN_WIDTH * y);
          for (i = 0; i < 80; i++) {
            if (!buffer[(newLoc + i) * 2]) {
              x = i;
              break;
            }
          }
        }
      } else x--;
      int loc = x + (SCREEN_WIDTH * y);
      buffer[loc * 2] = 0;
      buffer[loc * 2 + 1] = color;
    } else {
      int loc = x + (SCREEN_WIDTH * y);
      buffer[loc * 2] = theChar;
      buffer[loc * 2 + 1] = color;
      x++;
    }
    if (x >= SCREEN_WIDTH) {
      x = 0;
      y++;
    }
    while (y >= SCREEN_HEIGHT) {
      scrollUp();
      y--;
    }
  }
  setPosition(x, y);
  anlock_unlock(&printLock);
}

void printColor(uint8_t _color) {
  color = _color;
}

void printHex(unsigned long number) {
  const char * chars = "0123456789ABCDEF";
  unsigned char buf[32];
  unsigned char len = 0, i;
  do {
    unsigned char nextDig = (unsigned char)(number & 0xf);
    buf[len++] = chars[nextDig];
    number >>= 4;
  } while (number > 0);
  for (i = 0; i < len / 2; i++) {
    unsigned char a = buf[len - i - 1];
    buf[len - i - 1] = buf[i];
    buf[i] = a;
  }
  buf[len] = 0;
  print((const char *)buf);
}

void die(const char * msg) {
  print("[ERROR] ");
  print(msg);
  hang();
}

static void setPosition(unsigned short x, unsigned short y) {
  CURSOR_INFO[0] = x;
  CURSOR_INFO[1] = y;
  unsigned short position = (y * SCREEN_WIDTH) + x;
  // tell the VGA index register we are sending the `low` byte
  outb(0x3D4, 0x0f);
  outb(0x3D5, (unsigned char)(position & 0xff));
  // and now send the `high` byte
  outb(0x3D4, 0x0e);
  outb(0x3D5, (unsigned char)((position >> 8) & 0xff));

  buffer[1 + position * 2] = color;
}

static void scrollUp() {
  // copy the buffer into itself, one line up
  int i;
  for (i = 0; i < 2 * SCREEN_WIDTH * (SCREEN_HEIGHT - 1); i++) {
    buffer[i] = buffer[i + SCREEN_WIDTH * 2];
  }
  // clear the bottom line
  for (; i < 2 * SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
    buffer[i] = 0;
  }
}

