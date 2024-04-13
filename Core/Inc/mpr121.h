#include <stdint.h>

/*
 * Initializes the MPR121 at the specified I2C address
 * Returns 0 on success and -1 on failure.
 */
int mpr121_init(uint8_t addr);

/*
 * addr is a 7-bit address without r/w bit.
 * Returns 0 on success and -1 on failure.
 */
int mpr121_read(uint8_t addr, uint8_t reg_addr, uint8_t *data, int size);
int mpr121_write(uint8_t addr, uint8_t reg_addr, uint8_t *data, int size);

/*
 * The least significant 12 bits of the returned value represent touch status of the 12 electrodes.
 * Returns ~0 on failure;
 */
uint16_t mpr121_read_touch_status(uint8_t addr);
uint16_t mpr121_read_touch_status_nb(uint8_t addr);

/*
 * Sets the touch and release thresholds for all electrodes in an MPR121.
 * Returns 0 on success and -1 on failure.
 */
int mpr121_set_thresholds(uint8_t addr, uint8_t touch, uint8_t release);

int mpr121_read_nb(uint8_t addr, uint8_t reg_addr, uint8_t *data, int size);
