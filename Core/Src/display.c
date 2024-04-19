#include <math.h>
#include "stm32l4xx_hal.h"
#include "display.h"

#define PI 3.14159265
#define HSPI_WRITE16(s) trans_write(s)

#define HSPI_WRITE_PIXELS(c,l)  \
	for(uint32_t i=0; i<(l); i+=2) { \
		HSPI_WRITE16( *((uint16_t*)((uint8_t*)c + i)) ); \
	} \

extern SPI_HandleTypeDef hspi1;

static uint16_t htons(uint16_t n)
{
	uint16_t r = (n << 8) | (n >> 8);
	return r;
}


static void trans_write(uint16_t value)
{
	uint8_t tx[2] = {(uint8_t)((value>>8) & 0xFF), (uint8_t)((value) & 0xFF)};
	uint8_t rx[2] = {0};
	HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, HAL_MAX_DELAY);
}

static const uint8_t init_seq[] = {
        HX8357_SWRESET,
        0x80 + 100 / 5, // Soft reset, then delay 10 ms
        HX8357D_SETC,
        3,
        0xFF,
        0x83,
        0x57,
        0xFF,
        0x80 + 500 / 5, // No command, just delay 300 ms
        HX8357_SETRGB,
        4,
        0x80,
        0x00,
        0x06,
        0x06, // 0x80 enables SDO pin (0x00 disables)
        HX8357D_SETCOM,
        1,
        0x25, // -1.52V
        HX8357_SETOSC,
        1,
        0x68, // Normal mode 70Hz, Idle mode 55 Hz
        HX8357_SETPANEL,
        1,
        0x05, // BGR, Gate direction swapped
        HX8357_SETPWR1,
        6,
        0x00, // Not deep standby
        0x15, // BT
        0x1C, // VSPR
        0x1C, // VSNR
        0x83, // AP
        0xAA, // FS
        HX8357D_SETSTBA,
        6,
        0x50, // OPON normal
        0x50, // OPON idle
        0x01, // STBA
        0x3C, // STBA
        0x1E, // STBA
        0x08, // GEN
        HX8357D_SETCYC,
        7,
        0x02, // NW 0x02
        0x40, // RTN
        0x00, // DIV
        0x2A, // DUM
        0x2A, // DUM
        0x0D, // GDON
        0x78, // GDOFF
        HX8357D_SETGAMMA,
        34,
        0x02,
        0x0A,
        0x11,
        0x1d,
        0x23,
        0x35,
        0x41,
        0x4b,
        0x4b,
        0x42,
        0x3A,
        0x27,
        0x1B,
        0x08,
        0x09,
        0x03,
        0x02,
        0x0A,
        0x11,
        0x1d,
        0x23,
        0x35,
        0x41,
        0x4b,
        0x4b,
        0x42,
        0x3A,
        0x27,
        0x1B,
        0x08,
        0x09,
        0x03,
        0x00,
        0x01,
        HX8357_COLMOD,
        1,
        0x55, // 16 bit
        HX8357_MADCTL,
        1,
        0xC0,
        HX8357_TEON,
        1,
        0x00, // TW off
        HX8357_TEARLINE,
        2,
        0x00,
        0x02,
        HX8357_SLPOUT,
        0x80 + 150 / 5, // Exit Sleep, then delay 150 ms
        HX8357_DISPON,
        0x80 + 50 / 5, // Main screen turn on, delay 50 ms
        0,             // END OF COMMAND LIST
};


int disp_init()
{
	// Init SS and D/C to high
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, 1);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, 1);


	uint16_t data;

	const uint8_t *addr = init_seq;
	uint8_t cmd, x, numArgs;
	while ((cmd = *(addr++)) > 0) { // '0' command ends list
		x = *(addr++);
		numArgs = x & 0x7F;
		if (cmd != 0xFF) { // '255' is ignored
		  if (x & 0x80) {  // If high bit set, numArgs is a delay time
			disp_write(cmd, 0, 0);
		  } else {
			disp_write(cmd, addr, numArgs);
			addr += numArgs;
		  }
		}
		if (x & 0x80) {       // If high bit set...
		  HAL_Delay(numArgs * 5); // numArgs is actually a delay time (5ms units)
		}
	}

	disp_read(0x0A, &data, 2);
	//disp_write(0x22, 0, 0);
	// Clear out display
	disp_fill_rect(0, 0, HX8357_TFTWIDTH, HX8357_TFTHEIGHT, HX8357_WHITE);
	disp_fill_rect(HX8357_TFTWIDTH/2 - 1, 0, 2, HX8357_TFTHEIGHT, 0x0f2e);
	const uint16_t half_amp = 1*HX8357_TFTWIDTH/3;
	for (uint16_t x = 0; x < HX8357_TFTHEIGHT; x++) {
		uint16_t y = (uint16_t)((sin(10 * 2 * PI * x / HX8357_TFTHEIGHT) + sin(3 * 2 * PI * x / HX8357_TFTHEIGHT))/2 * half_amp + HX8357_TFTWIDTH/2);
		disp_fill_rect(y, x, 3, 3, 0x03bf);
	}

}

inline void disp_set_pixel(uint16_t x, uint16_t y, uint16_t color)
{
	disp_fill_rect(x, y, 1, 1, color);
}

void disp_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
	uint16_t x_win[] = {htons(x), htons(x + width - 1)};
	uint16_t y_win[] = {htons(y), htons(y + height - 1)};
	disp_write(HX8357_CASET, x_win, 4);
	disp_write(HX8357_PASET, y_win, 4);

	color = htons(color);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, 0);
	disp_write_cmd(HX8357_RAMWR);
	for (int i = 0; i < height * width; i++) {
		disp_write_data(&color, 2);
	}
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, 1);
}


void disp_read(uint8_t cmd, void *data, int len)
{
	// Set SS to low
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, 0);

	// Write command to display
	disp_write_cmd(cmd);

	// Write data to display
	disp_read_data(data, len);

	// Set SS to high
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, 1);
}


void disp_write(uint8_t cmd, void *data, int len)
{
	// Set SS to low
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, 0);

	// Write command to display
	disp_write_cmd(cmd);

	// Write data to display
	disp_write_data(data, len);

	// Set SS to high
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, 1);
}

/*
 * Writes a command to the display
 */
void disp_write_cmd(uint8_t cmd)
{

	// Set D/C to low
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, 0);

	// Write cmd
	HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
	// Set D/C to high
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, 1);
}

/*
 * Writes data to the display
 */
void disp_write_data(void *data, int len)
{
	if (len == 0) return;

	// Set D/C to high
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, 1);

	// Write data
	HAL_SPI_Transmit(&hspi1, (uint8_t *) data, len, HAL_MAX_DELAY);
}

/*
 * Reads values from the display
 */
void disp_read_data(void *data, int len)
{
	if (len == 0) return;

	// Set D/C to high
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, 1);

	// Write data
	HAL_SPI_Receive(&hspi1, (uint8_t *) data, len, HAL_MAX_DELAY);
}
