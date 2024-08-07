/*
ProFont
MIT License

Copyright (c) 2014 Gareth Redman, Carl Osterwald, Stephen C. Gilardi, Andrew Welch

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

// ProFont 6x11, glyphs 32-126

#include <stdint.h>

#define FONT_WIDTH 6
#define FONT_HEIGHT 11

const uint8_t font[1045] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x00, 0x20, 0x00, 0x00, 0x00, 0x50, 0x50, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x50, 0xf8, 0x50, 0xf8, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x70, 0xa8,
  0xa0, 0x70, 0x28, 0xa8, 0x70, 0x20, 0x00, 0x00, 0x00, 0x78, 0xa8, 0xb0, 0x50, 0x68, 0xa8, 0x90,
  0x00, 0x00, 0x00, 0x00, 0x60, 0x90, 0xa0, 0x40, 0xa8, 0x90, 0x68, 0x00, 0x00, 0x00, 0x20, 0x20,
  0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x20, 0x40, 0x40, 0x40, 0x40, 0x40,
  0x20, 0x10, 0x00, 0x00, 0x40, 0x20, 0x10, 0x10, 0x10, 0x10, 0x10, 0x20, 0x40, 0x00, 0x00, 0x00,
  0x20, 0xa8, 0x70, 0xa8, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0xf8, 0x20,
  0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0x20, 0x40, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x08, 0x08, 0x10, 0x10, 0x20, 0x20, 0x40, 0x40, 0x80, 0x80,
  0x00, 0x00, 0x70, 0x88, 0x98, 0xa8, 0xc8, 0x88, 0x70, 0x00, 0x00, 0x00, 0x00, 0x20, 0xe0, 0x20,
  0x20, 0x20, 0x20, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x70, 0x88, 0x08, 0x10, 0x20, 0x40, 0xf8, 0x00,
  0x00, 0x00, 0x00, 0x70, 0x88, 0x08, 0x30, 0x08, 0x88, 0x70, 0x00, 0x00, 0x00, 0x00, 0x10, 0x30,
  0x50, 0x90, 0xf8, 0x10, 0x38, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x80, 0xf0, 0x08, 0x08, 0x88, 0x70,
  0x00, 0x00, 0x00, 0x00, 0x70, 0x80, 0xf0, 0x88, 0x88, 0x88, 0x70, 0x00, 0x00, 0x00, 0x00, 0xf8,
  0x08, 0x08, 0x10, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x70, 0x88, 0x88, 0x70, 0x88, 0x88,
  0x70, 0x00, 0x00, 0x00, 0x00, 0x70, 0x88, 0x88, 0x88, 0x78, 0x08, 0x70, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x30, 0x30, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0x00,
  0x60, 0x60, 0x20, 0x40, 0x00, 0x00, 0x08, 0x10, 0x20, 0x40, 0x20, 0x10, 0x08, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xf8, 0x00, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x20, 0x10, 0x08,
  0x10, 0x20, 0x40, 0x00, 0x00, 0x00, 0x00, 0x70, 0x88, 0x08, 0x10, 0x20, 0x00, 0x20, 0x00, 0x00,
  0x00, 0x00, 0x70, 0x88, 0xb8, 0xa8, 0xb8, 0x80, 0x78, 0x00, 0x00, 0x00, 0x00, 0x20, 0x50, 0x50,
  0x88, 0xf8, 0x88, 0x88, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x88, 0x88, 0xf0, 0x88, 0x88, 0xf0, 0x00,
  0x00, 0x00, 0x00, 0x70, 0x88, 0x80, 0x80, 0x80, 0x88, 0x70, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x88,
  0x88, 0x88, 0x88, 0x88, 0xf0, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x80, 0x80, 0xf0, 0x80, 0x80, 0xf8,
  0x00, 0x00, 0x00, 0x00, 0xf8, 0x80, 0x80, 0xf0, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x70,
  0x88, 0x80, 0x98, 0x88, 0x88, 0x70, 0x00, 0x00, 0x00, 0x00, 0x88, 0x88, 0x88, 0xf8, 0x88, 0x88,
  0x88, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x20, 0x20, 0x20, 0x20, 0x20, 0xf8, 0x00, 0x00, 0x00, 0x00,
  0x08, 0x08, 0x08, 0x08, 0x88, 0x88, 0x70, 0x00, 0x00, 0x00, 0x00, 0x88, 0x90, 0xa0, 0xc0, 0xa0,
  0x90, 0x88, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xf8, 0x00, 0x00, 0x00,
  0x00, 0x88, 0xd8, 0xa8, 0xa8, 0x88, 0x88, 0x88, 0x00, 0x00, 0x00, 0x00, 0x88, 0xc8, 0xa8, 0x98,
  0x88, 0x88, 0x88, 0x00, 0x00, 0x00, 0x00, 0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0x00, 0x00,
  0x00, 0x00, 0xf0, 0x88, 0x88, 0xf0, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x70, 0x88, 0x88,
  0x88, 0x88, 0xa8, 0x70, 0x08, 0x00, 0x00, 0x00, 0xf0, 0x88, 0x88, 0xf0, 0x88, 0x88, 0x88, 0x00,
  0x00, 0x00, 0x00, 0x70, 0x88, 0x80, 0x70, 0x08, 0x88, 0x70, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x20,
  0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70,
  0x00, 0x00, 0x00, 0x00, 0x88, 0x88, 0x88, 0x50, 0x50, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x88,
  0x88, 0x88, 0xa8, 0xa8, 0xd8, 0x88, 0x00, 0x00, 0x00, 0x00, 0x88, 0x88, 0x50, 0x20, 0x50, 0x88,
  0x88, 0x00, 0x00, 0x00, 0x00, 0x88, 0x88, 0x88, 0x50, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00,
  0xf8, 0x08, 0x10, 0x20, 0x40, 0x80, 0xf8, 0x00, 0x00, 0x00, 0x30, 0x20, 0x20, 0x20, 0x20, 0x20,
  0x20, 0x20, 0x30, 0x00, 0x00, 0x40, 0x40, 0x20, 0x20, 0x10, 0x10, 0x08, 0x08, 0x04, 0x04, 0x00,
  0x60, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x60, 0x00, 0x00, 0x00, 0x20, 0x50, 0x88, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc,
  0x00, 0x40, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78,
  0x88, 0x88, 0x98, 0x68, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0xf0, 0x88, 0x88, 0x88, 0xf0, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x88, 0x80, 0x80, 0x78, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08,
  0x78, 0x88, 0x88, 0x88, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x88, 0xf8, 0x80, 0x78,
  0x00, 0x00, 0x00, 0x00, 0x18, 0x20, 0x70, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x78, 0x88, 0x88, 0x88, 0x78, 0x08, 0x70, 0x00, 0x00, 0x80, 0x80, 0xf0, 0x88, 0x88, 0x88,
  0x88, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x60, 0x20, 0x20, 0x20, 0x70, 0x00, 0x00, 0x00, 0x00,
  0x20, 0x00, 0x60, 0x20, 0x20, 0x20, 0x20, 0x20, 0xc0, 0x00, 0x00, 0x80, 0x80, 0x90, 0xa0, 0xe0,
  0x90, 0x88, 0x00, 0x00, 0x00, 0x00, 0x60, 0x20, 0x20, 0x20, 0x20, 0x20, 0x70, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0xf0, 0xa8, 0xa8, 0xa8, 0xa8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb0, 0xc8,
  0x88, 0x88, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x88, 0x88, 0x88, 0x70, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xf0, 0x88, 0x88, 0x88, 0xf0, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x78,
  0x88, 0x88, 0x88, 0x78, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0xb0, 0xc8, 0x80, 0x80, 0x80, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x80, 0x70, 0x08, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20,
  0x70, 0x20, 0x20, 0x20, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x88, 0x88, 0x98, 0x68,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x88, 0x50, 0x50, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xa8, 0xa8, 0xa8, 0xa8, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x50, 0x20, 0x50,
  0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x88, 0x88, 0x88, 0x78, 0x08, 0x70, 0x00, 0x00,
  0x00, 0x00, 0xf8, 0x10, 0x20, 0x40, 0xf8, 0x00, 0x00, 0x10, 0x20, 0x20, 0x20, 0x20, 0x40, 0x20,
  0x20, 0x20, 0x20, 0x10, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x40,
  0x20, 0x20, 0x20, 0x20, 0x10, 0x20, 0x20, 0x20, 0x20, 0x40, 0x00, 0x00, 0x00, 0x00, 0x68, 0xb0,
  0x00, 0x00, 0x00, 0x00, 0x00
};
