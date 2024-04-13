#include "stm32l4xx_hal.h"
#include "mpr121.h"

#define ECR 0x5E
#define SOFT_RST 0x80
#define TOUCH_STATUS 0x00
#define RST_VAL 0x63
#define ECR_ALL_ENABLE 0b10001100
#define NUM_ELECS 12
#define TOUCH_THRESHOLD 0x30
#define RELEASE_THRESHOLD 0x08

extern I2C_HandleTypeDef hi2c1;

int mpr121_init(uint8_t addr)
{
	uint8_t data;

	// resetting ECR
	data = 0;
	if (mpr121_write(addr, ECR, &data, 1) != 0) {
		return -1;
	}

	// soft resetting
	data = RST_VAL;
	if (mpr121_write(addr, SOFT_RST, &data, 1)) {
		return -1;
	}
	HAL_Delay(1);

	// checking whether reset actually worked by reading config reg 2's default value
	if (mpr121_read(addr, 0x5d, &data, 1) || data != 0x24) {
		return -1;
	}

	// set default touch and release thresholds
	if (mpr121_set_thresholds(addr, TOUCH_THRESHOLD, RELEASE_THRESHOLD)) {
		return -1;
	}

	// Enabling all electrodes with baseline tracking upon initialisation
	data = ECR_ALL_ENABLE;
	if (mpr121_write(addr, ECR, &data, 1)) {
		return -1;
	}

	return 0;
}

int mpr121_set_thresholds(uint8_t addr, uint8_t touch, uint8_t release)
{
	uint8_t thresholds[] = {touch, release};
	for (int i = 0; i < 2*NUM_ELECS; i += 2) {
		// touch
		if (mpr121_write(addr, 0x41 + i, thresholds, 1)) {
			return -1;
		}
		// release
		if (mpr121_write(addr, 0x42 + i, thresholds+1, 1)) {
			return -1;
		}

	}
	return 0;
}

int mpr121_read(uint8_t addr, uint8_t reg_addr, uint8_t *data, int size)
{
	return HAL_I2C_Mem_Read(&hi2c1, (addr << 1) | 1, reg_addr, I2C_MEMADD_SIZE_8BIT, data, size, HAL_MAX_DELAY);
}

int mpr121_read_nb(uint8_t addr, uint8_t reg_addr, uint8_t *data, int size)
{
	return HAL_I2C_Mem_Read_IT(&hi2c1, (addr << 1) | 1, reg_addr, I2C_MEMADD_SIZE_8BIT, data, size);
}


int mpr121_write(uint8_t addr, uint8_t reg_addr, uint8_t *data, int size)
{
	I2C_HandleTypeDef *sdf = &hi2c1;
	return HAL_I2C_Mem_Write(&hi2c1, (addr << 1), reg_addr, I2C_MEMADD_SIZE_8BIT, data, size, HAL_MAX_DELAY);
}

uint16_t mpr121_read_touch_status(uint8_t addr)
{
	uint8_t status[2] = {0};
	if (mpr121_read(addr, TOUCH_STATUS, status, 2)) {
		return ~0;

	}
	uint16_t ret = status[1];
	ret <<= 8;
	ret |= status[0];
	return ret;
}

uint16_t mpr121_read_touch_status_nb(uint8_t addr)
{
	uint8_t status[2] = {0};
	if (mpr121_read_nb(addr, TOUCH_STATUS, status, 2)) {
		return ~0;

	}
	uint16_t ret = status[1];
	ret <<= 8;
	ret |= status[0];
	return ret;
}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
//	while (HAL_I2C_GetState(&hi2c1) != HAL_I2C_STATE_READY)
//	{
//	}
	uint16_t touch_status = mpr121_read_touch_status_nb(0x5A);
}
