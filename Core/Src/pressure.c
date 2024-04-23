#include "stm32l4xx_hal.h"
#include "pressure.h"

#define LH_ADDR_LSB 0xa5
#define RH_ADDR_LSB 0xa5

extern UART_HandleTypeDef huart3;

static uint8_t frame[28];
static uint8_t ibuf[28];

extern uint16_t l_pressure, r_pressure;
extern int best_index;
extern int pressure_wait;

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
	HAL_UART_Receive_IT(&huart3, ibuf, 28);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	uint8_t *frame = 0;
	uint8_t *buf = ibuf;

	if(buf[0] != 0x7E){
		goto exit;
	}

	if(frame == 0){
		goto exit;
	}

	uint64_t addr_lsb = frame[11];

	best_index = -1;
	uint16_t max_pressure = 0;
	for (int i = 0; i < 5; i++){
		uint16_t pressure = 0x3ff - nread16(frame + 17 + 2 * i);
		if (pressure > max_pressure && pressure != 0x00){
			best_index = i;
			max_pressure = pressure;
		}
	}
	pressure_wait = 0;


exit:
	frame[0] = 0;
	pressure_read_start();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
	if (huart->ErrorCode & HAL_UART_ERROR_ORE) {
		__HAL_UART_CLEAR_OREFLAG(&huart3);
		HAL_UART_Receive_IT(&huart3, frame, 28);
	} else {
		// ERROR
		// Do other errors need to be cleared?
		volatile int a = 5;
	}
}
