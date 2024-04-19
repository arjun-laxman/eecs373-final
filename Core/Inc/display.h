#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

// Ported from adafruit lib
#define HX8357D                    0xD
#define HX8357B                    0xB

#define HX8357_TFTWIDTH            320
#define HX8357_TFTHEIGHT           480

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
#define HX8357B_PTLON             0x12
#define HX8357B_NORON             0x13

#define HX8357_INVOFF             0x20
#define HX8357_INVON              0x21
#define HX8357_DISPOFF            0x28
#define HX8357_DISPON             0x29

#define HX8357_CASET              0x2A
#define HX8357_PASET              0x2B
#define HX8357_RAMWR              0x2C
#define HX8357_RAMRD              0x2E

#define HX8357B_PTLAR             0x30
#define HX8357_TEON               0x35
#define HX8357_TEARLINE           0x44
#define HX8357_MADCTL             0x36
#define HX8357_COLMOD             0x3A

#define HX8357_SETOSC             0xB0
#define HX8357_SETPWR1            0xB1
#define HX8357B_SETDISPLAY        0xB2
#define HX8357_SETRGB             0xB3
#define HX8357D_SETCOM            0xB6

#define HX8357B_SETDISPMODE       0xB4
#define HX8357D_SETCYC            0xB4
#define HX8357B_SETOTP            0xB7
#define HX8357D_SETC              0xB9

#define HX8357B_SET_PANEL_DRIVING 0xC0
#define HX8357D_SETSTBA           0xC0
#define HX8357B_SETDGC            0xC1
#define HX8357B_SETID             0xC3
#define HX8357B_SETDDB            0xC4
#define HX8357B_SETDISPLAYFRAME   0xC5
#define HX8357B_GAMMASET          0xC8
#define HX8357B_SETCABC           0xC9
#define HX8357_SETPANEL           0xCC

#define HX8357B_SETPOWER          0xD0
#define HX8357B_SETVCOM           0xD1
#define HX8357B_SETPWRNORMAL      0xD2

#define HX8357B_RDID1             0xDA
#define HX8357B_RDID2             0xDB
#define HX8357B_RDID3             0xDC
#define HX8357B_RDID4             0xDD

#define HX8357D_SETGAMMA          0xE0

#define HX8357B_SETGAMMA          0xC8
#define HX8357B_SETPANELRELATED   0xE9

// Color definitions
#define	HX8357_BLACK   0x0000 ///< BLACK color for drawing graphics
#define	HX8357_BLUE    0x001F ///< BLUE color for drawing graphics
#define	HX8357_RED     0xF800 ///< RED color for drawing graphics
#define	HX8357_GREEN   0x07E0 ///< GREEN color for drawing graphics
#define HX8357_CYAN    0x07FF ///< CYAN color for drawing graphics
#define HX8357_MAGENTA 0xF81F ///< MAGENTA color for drawing graphics
#define HX8357_YELLOW  0xFFE0 ///< YELLOW color for drawing graphics
#define HX8357_WHITE   0xFFFF ///< WHITE color for drawing graphics

int disp_init();

void disp_write(uint8_t cmd, void *data, int len);

void disp_read(uint8_t cmd, void *data, int len);

/*
 * Writes a command to the display
 */
void disp_write_cmd(uint8_t cmd);

/*
 * Writes data to the display
 */
void disp_write_data(void *data, int len);

/*
 * Reads values from the display
 */
void disp_read_data(void *data, int len);

#endif
