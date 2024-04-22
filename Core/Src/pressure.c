#include "stm32l4xx_hal.h"
#include "pressure.h"

#define LH_ADDR_LSB 0xa5
#define RH_ADDR_LSB 0xa5

extern UART_HandleTypeDef huart3;

static uint8_t frame[20];
static uint8_t ibuf[40];

extern uint16_t l_pressure, r_pressure;


static inline uint64_t nread64(uint8_t *p)
{
	uint64_t r = p[0];
	for (int i = 1; i < 8; i++) {
		r = (r << 8) | p[i];
	}
	return r;
}

static inline uint16_t nread16(uint8_t *p)
{
	uint16_t r = p[0];
	return (r << 8) | p[1];
}

void pressure_read_start()
{
	HAL_UART_Receive_IT(&huart3, ibuf, 40);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	uint8_t *frame = 0;
	uint8_t *buf = ibuf;
start:
	for(int i = 0; i < 20; i++) {
		if(buf[i] == 0x7E) {
			frame = buf + i;
			break;
		}
	}
	if(frame == 0){
		goto exit;
	}

	uint64_t addr_lsb = frame[11];
	uint16_t left_adc_val = 0;
	uint16_t right_adc_val = 0;

	if (addr_lsb == LH_ADDR_LSB) {
		left_adc_val = nread16(frame + 17);
	} else if (addr_lsb == RH_ADDR_LSB){
		right_adc_val  = nread16(frame + 17);
	} else {
		l_pressure = 0;
		r_pressure = 0;
	}
	if (frame == ibuf) {
		buf = ibuf + 20;
		goto start;
	}

exit:
	frame[0] = 0;
	pressure_read_start();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	if (huart->ErrorCode & HAL_UART_ERROR_ORE) {
		__HAL_UART_CLEAR_OREFLAG(&huart3);
		HAL_UART_Receive_IT(&huart3, frame, 20);
	} else {
		// ERROR
		// Do other errors need to be cleared?
		volatile int a = 5;
	}
}
