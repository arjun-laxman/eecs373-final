#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#define LANDSCAPE

#ifdef LANDSCAPE
#define DISP_HEIGHT HX8357_WIDTH
#define DISP_WIDTH HX8357_HEIGHT
#else
#define DISP_HEIGHT HX8357_HEIGHT
#define DISP_WIDTH HX8357_WIDTH
#endif

#define CHAR_HEIGHT 7
#define CHAR_WIDTH 5
#define CHAR_PADDING 1

// From Adafruit Library
#define HX8357_WIDTH              320
#define HX8357_HEIGHT             480

#define HX8357_NOP                0x00
#define HX8357_SWRESET            0x01
#define HX8357_RDDID              0x04
#define HX8357_RDDST              0x09
#define HX8357_RDPOWMODE          0x0A
#define HX8357_RDMADCTL           0x0B
#define HX8357_RDCOLMOD           0x0C
#define HX8357_RDDIM              0x0D
#define HX8357_RDDSDR             0x0F
#define HX8357_SLPIN              0x10
#define HX8357_SLPOUT             0x11
#define HX8357_INVOFF             0x20
#define HX8357_INVON              0x21
#define HX8357_DISPOFF            0x28
#define HX8357_DISPON             0x29
#define HX8357_CASET              0x2A
#define HX8357_PASET              0x2B
#define HX8357_RAMWR              0x2C
#define HX8357_RAMRD              0x2E
#define HX8357_TEON               0x35
#define HX8357_TEARLINE           0x44
#define HX8357_MADCTL             0x36
#define HX8357_COLMOD             0x3A
#define HX8357_SETOSC             0xB0
#define HX8357_SETPWR1            0xB1
#define HX8357_SETRGB             0xB3
#define HX8357D_SETCOM            0xB6
#define HX8357D_SETCYC            0xB4
#define HX8357D_SETC              0xB9
#define HX8357D_SETSTBA           0xC0
#define HX8357_SETPANEL           0xCC
#define HX8357D_SETGAMMA          0xE0

// Color definitions
#define	BLACK   0x0000
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

/*
 * Initializes display.
 */
int disp_init();

/*
 * Sends out a command and writes len bytes of data.
 */
void disp_write(uint8_t cmd, const void *data, int len);

/*
 * Sends out a command and reads len bytes into data.
 */
void disp_read(uint8_t cmd, void *data, int len);

/*
 * Sets a pixel at (x,y)
 */
inline void disp_set_pixel(uint16_t x, uint16_t y, uint16_t color);

/*
 * Fills a rectangle with a color.
 * Top right coordinates are at (x,y).
 */
void disp_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);

/*
 * Draws a line from (x1,y1) to (x2,y2) using Bresenham's line algorithm.
 */
void disp_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t size, uint16_t color);

/*
 * Prints a string on the screen at (x,y).
 */
void disp_print(char *s, uint16_t x, uint16_t y, uint8_t size, uint16_t fg, uint16_t bg);

/*
 * Prints one character on the screen at (x,y).
 */
void disp_print_char(char c, uint16_t x, uint16_t y, uint8_t size, uint16_t fg, uint16_t bg);

#endif
