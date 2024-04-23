#include <math.h>
#include "stm32l4xx_hal.h"
#include "font7x5.h"
#include "display.h"

#define DMA_THRESHOLD 4
#define DMA_BUF_SIZE 256
#define PI 3.14159265

extern SPI_HandleTypeDef hspi1;

static int dma_busy = 0;
static uint8_t dma_buf[DMA_BUF_SIZE];

#define SWAPU16(a, b) {\
	uint16_t tmp = (a); \
	(a) = (b); \
	(b) = tmp; \
}

static uint16_t htons(uint16_t n)
{
	uint16_t r = (n << 8) | (n >> 8);
	return r;
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

/*
 * Writes a command to the display
 */
static void disp_write_cmd(uint8_t cmd)
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
static void disp_write_data(const void *data, int len)
{
	if (len == 0) return;

	// Set D/C to high
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, 1);


	while (dma_busy);
	memcpy(dma_buf, data, len);
	// Write data
	if (len >= DMA_THRESHOLD) {
		// Use DMA
		//dma_busy = 1;
		//HAL_SPI_Transmit_DMA(&hspi1, (uint8_t *) dma_buf, len);
		HAL_SPI_Transmit(&hspi1, (uint8_t *) data, len, HAL_MAX_DELAY);
	} else {
		HAL_SPI_Transmit(&hspi1, (uint8_t *) data, len, HAL_MAX_DELAY);
	}
}

/*
 * Reads values from the display
 */
static void disp_read_data(void *data, int len)
{
	if (len == 0) return;

	// Set D/C to high
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8, 1);

	// Write data
	HAL_SPI_Receive(&hspi1, (uint8_t *) data, len, HAL_MAX_DELAY);
}

/*
 * If mode is landscape, converts landscape coordinates to
 * base display portrait coordinates.
 */
static inline void orient_rect(uint16_t *x, uint16_t *y, uint16_t *width, uint16_t *height)
{
#ifdef LANDSCAPE
	uint16_t new_x = HX8357_WIDTH - (*y + *height);
	uint16_t new_y = *x;
	uint16_t new_width = *height;
	uint16_t new_height = *width;
	*x = new_x;
	*y = new_y;
	*width = new_width;
	*height = new_height;
#endif
}

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

	// Clear out display
	disp_fill_rect(0, 0, DISP_WIDTH, DISP_HEIGHT, BLACK);
	const uint16_t half_amp = DISP_HEIGHT/4;
	uint16_t prev_x = 0, prev_y = 2*DISP_HEIGHT / 3;

	uint16_t red = 0xf81c;
	uint16_t blue = 0x0dff;
	uint16_t green = 0x0f6f;

	prev_x = 0;
	prev_y = 2*DISP_HEIGHT / 3;
	for (uint16_t x = 0; x < DISP_WIDTH; x += 4) {
		uint16_t y = (uint16_t)((sin(10 * 2 * PI * x / DISP_WIDTH) + sin(3 * 2 * PI * x / DISP_WIDTH))/2 * half_amp + 2*DISP_HEIGHT/3);
		disp_draw_line(prev_x, prev_y, x, y, 4, blue);
		prev_x = x;
		prev_y = y;
	}
	prev_x = 0;
	prev_y = 2*DISP_HEIGHT / 3;
	for (uint16_t x = 0; x < DISP_WIDTH; x += 4) {
		uint16_t y = (uint16_t)((sin(7 * 2 * PI * x / DISP_WIDTH) + sin(4 * 2 * PI * x / DISP_WIDTH))/2 * half_amp + 2*DISP_HEIGHT/3);
		disp_draw_line(prev_x, prev_y, x, y, 4, red); // Red
		prev_x = x;
		prev_y = y;
	}

	disp_print("Roll Over Beethoven", 20, 40, 4, green, BLACK);



}

inline void disp_set_pixel(uint16_t x, uint16_t y, uint16_t color)
{
	disp_fill_rect(x, y, 1, 1, color);
}

void disp_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{

	orient_rect(&x, &y, &width, &height);

	// Check input
	if (x >= HX8357_WIDTH || y >= HX8357_HEIGHT) {
		return;
	}
	width = (width > HX8357_WIDTH - x) ? HX8357_WIDTH - x : width;
	height = (width > HX8357_HEIGHT - y) ? HX8357_HEIGHT - y : height;

	// Set window
	uint16_t x_win[] = {htons(x), htons(x + width - 1)};
	uint16_t y_win[] = {htons(y), htons(y + height - 1)};
	disp_write(HX8357_CASET, x_win, 4);
	disp_write(HX8357_PASET, y_win, 4);

	uint8_t buf[DMA_BUF_SIZE];

	// Write color
	color = htons(color);
	int total_bytes = height * width * 2;
	int buf_fill_len = (total_bytes > DMA_BUF_SIZE) ? DMA_BUF_SIZE : total_bytes;

	for (int i = 0; i < buf_fill_len; i += 2) {
		*((uint16_t *) (buf + i)) = color;
	}

	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, 0);
	disp_write_cmd(HX8357_RAMWR);

	while (total_bytes > 0) {
		uint16_t write_count = total_bytes > DMA_BUF_SIZE ? DMA_BUF_SIZE : total_bytes;
		disp_write_data(&buf, write_count);
		total_bytes -= write_count;
	}
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_6, 1);
}

void disp_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint8_t size, uint16_t color)
{
	// Check inputs
	if (x2 >= DISP_WIDTH || y2 >= DISP_HEIGHT) {
		return;
	}
	// Just fill rectangle fast for horizontal/vertical lines
	if (x1 == x2) {
		if (y1 > y2) SWAPU16(y1, y2);
		disp_fill_rect(x1, y1, size, y2 - y1 + 1, color);
		return;
	}
	if (y1 == y2) {
		if (x1 > x2) SWAPU16(x1, x2);
		disp_fill_rect(x1, y1, x2 - x1 + 1, size, color);
		return;
	}

	int dx = abs(x2 - x1);
	int dy = -abs(y2 - y1);
	int sx = x1 < x2 ? 1 : -1;
	int sy = y1 < y2 ? 1 : -1;
	int err = dx + dy;

	// Draw line
	while (1) {
		disp_fill_rect(x1, y1, size, size, color);
		if (x1 == x2 && y1 == y2) break;

		int err2 = err * 2;
		if (err2 >= dy) {
			if (x1 == x2) break;
			err += dy;
			x1 += sx;
		}
		if (err2 <= dx) {
			if (y1 == y2) break;
			err += dx;
			y1 += sy;
		}
	}

}

void disp_print_char(char c, uint16_t x, uint16_t y, uint8_t size, uint16_t fg, uint16_t bg)
{
	for (int i = 0; i < CHAR_WIDTH; i++) {
		uint8_t col = font7x5[5*c + i];
		for (int j = 0; j < CHAR_HEIGHT; j++) {
			if (col & 1) {
				disp_fill_rect(x + size*i, y + size*j, size, size, fg);
			} else {
				disp_fill_rect(x + size*i, y + size*j, size, size, bg);
			}
			col >>= 1;
		}
	}
}

void disp_print(char *s, uint16_t x, uint16_t y, uint8_t size, uint16_t fg, uint16_t bg)
{
	while (*s) {
		disp_print_char(*s, x, y, size, fg, bg);
		s++;
		x += (CHAR_WIDTH + CHAR_PADDING)*size;
	}

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


void disp_write(uint8_t cmd, const void *data, int len)
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

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
	dma_busy = 0;
}


